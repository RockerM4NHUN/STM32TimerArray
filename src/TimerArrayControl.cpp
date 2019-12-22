#include "TimerArrayControl.hpp"

// capture update events and fire the timer array's callback chain
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef* htim){
    TAC_CallbackChain::fire(htim);
}

// -----                                  -----
// ----- TimerArrayControl implementation -----
// -----                                  -----


TimerArrayControl::TimerArrayControl(TIM_HandleTypeDef *const htim, const uint32_t fclk, const uint32_t clkdiv, const uint32_t bits) : 
    fclk(fclk),
    clkdiv(clkdiv),
    bits(bits),
    tickCallback([=](TIM_HandleTypeDef* _htim){
        if (this->htim == _htim) tick();
    }),
    htim(htim),
    timerString(Timer(nullptr)),
    request(REQUEST_NONE),
    requestTimer(nullptr),
    requestDelay(0),
    isTickOngoing(false)
{}

void TimerArrayControl::begin(){

    // stop timer if it was running
    HAL_TIM_OC_Stop_IT(htim, TARGET_CC_CHANNEL);

    htim->Init.CounterMode = TIM_COUNTERMODE_UP; // all STM32 counters support it
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE; // by disabling, write to ARR shadow regs happens immedietely
    htim->Init.Period = max_count; // set max period for maximum amount of possible delay
    htim->Init.Prescaler = prescaler - 1; // prescaler divides clock by psc+1
    htim->Init.ClockDivision =
        (prediv == 1) ? (
            TIM_CLOCKDIVISION_DIV1
        ) : (
            (prediv == 2) ? (
                TIM_CLOCKDIVISION_DIV2
            ) : (
                TIM_CLOCKDIVISION_DIV4
            )
        );

    TIM_OC_InitTypeDef oc_init;
    oc_init.OCMode = TIM_OCMODE_TIMING;

    HAL_TIM_OC_Init(htim);
    HAL_TIM_OC_ConfigChannel(htim, &oc_init, TARGET_CC_CHANNEL);
    uint32_t cnt = __HAL_TIM_GET_COUNTER(htim);
    uint32_t target = timerString.next == nullptr ? (max_count & (cnt-1)) : timerString.next->target;
    __HAL_TIM_SET_COMPARE(htim, TARGET_CC_CHANNEL, target); // if no timers to fire yet, set max delay between unneeded interrupts
    HAL_TIM_OC_Start_IT(htim, TARGET_CC_CHANNEL);
}

/**
 * This method can only be called from interupts.
 * */
void TimerArrayControl::tick(){

    isTickOngoing = true;

    uint32_t cnt = __HAL_TIM_GET_COUNTER(htim);
    static const auto CALLBACK_JITTER = 1000;

    // handle request
    switch(request){
        case REQUEST_ATTACH: registerAttachedTimer(cnt); break;
        case REQUEST_DETACH: registerDetachedTimer(); break;
        case REQUEST_DELAY_CHANGE: registerDelayChange(); break;
        default: break;
    }
    request = REQUEST_NONE;

    // handle timeout
    while (timerString.next && ((uint32_t)(cnt - timerString.next->target)) < CALLBACK_JITTER){
        Timer* timer = timerString.next;

        // set up the next interrupt generation
        if (timer->isPeriodic){

            // set new target for timer
            timer->target += timer->delay;
            timer->target &= max_count;

            // find fitting place for timer in string
            Timer* ins = timer;
            while(ins->next && ins->next->target < timer->target){
                // while there are more timers and the next timer's target is sooner than the modified one's
                // advance |ins| on the timer string
                ins = ins->next;
            }

            if (ins != timer){
                // remove our timer from the string
                timerString.next = timer->next;

                // insert our timer between |ins| and next of |ins|
                timer->next = ins->next;
                ins->next = timer;
            }

        } else {
            // if timer is not periodic, it is done, we can detach it
            Timer* timer = timerString.next;
            timer->running = false;
            timerString.next = timer->next;
            timer->next = nullptr;
        }

        // set the new target
        uint32_t target = timerString.next == nullptr ? (max_count & (cnt - 1)) : timerString.next->target;
        __HAL_TIM_SET_COMPARE(htim, TARGET_CC_CHANNEL, target);

        // fire callback
        timer->f();
    }

    isTickOngoing = false;
}

void TimerArrayControl::registerAttachedTimer(uint32_t cnt){

    // if timer is already attached to a controller, do nothing
    if (requestTimer->running) return;

    // get current time in ticks and add the requested delay to find the target time
    requestTimer->target = max_count & (requestTimer->delay + cnt);

    // find fitting place for timer in string
    Timer* it = &timerString;
    while(it->next && it->next->target < requestTimer->target){
        // while there are more timers and the next timer's target is sooner than the new one's
        // advance it on the timer string
        it = it->next;
    }

    // insert the new timer between it and next of it
    requestTimer->running = true;
    requestTimer->next = it->next;
    it->next = requestTimer;

    // if the first timer changed, adjust interrupt target
    if (timerString.next == requestTimer) __HAL_TIM_SET_COMPARE(htim, TARGET_CC_CHANNEL, requestTimer->target);
}

void TimerArrayControl::registerDetachedTimer(){
    if (!requestTimer->running) return;
    Timer* it = &timerString;

    while(it->next && it->next != requestTimer) it = it->next;

    if (it->next == requestTimer){
        it->next = requestTimer->next;
        requestTimer->next = nullptr;
        requestTimer->running = false;
    }
}

void TimerArrayControl::registerDelayChange(){

    if (!requestTimer->running) {
        requestTimer->delay = requestDelay;
        return;
    }

    requestTimer->target += requestDelay - requestTimer->delay;
    requestTimer->target &= max_count; 
    requestTimer->delay = requestDelay;
    
    // find fitting place for timer in string
    Timer* ins = &timerString;
    Timer* rem = ins;
    while(ins->next && ins->next->target < requestTimer->target){
        // while there are more timers and the next timer's target is sooner than the modified one's
        // advance |ins| on the timer string
        ins = ins->next;

        // if the next timer is not our's to remove, advance |rem| on the string
        if (rem->next != requestTimer) rem = ins;
    }
    while(rem->next && rem->next != requestTimer) rem = rem->next;
    
    // if the interrupt was set to a timer that has changed, set new target
    // if ins is first timer, the timer was put to first place
    // if rem is first timer, the timer was moved from first place
    // if both, the first timers target was probably changed
    // in all cases new target is needed
    if (&timerString == ins || &timerString == rem) {
        __HAL_TIM_SET_COMPARE(htim, TARGET_CC_CHANNEL, timerString.next->target);
    }

    // only move timer if the timer's place changed
    if (ins != rem){
        // remove our timer from the string
        rem->next = requestTimer->next;

        // insert our timer between |ins| and next of |ins|
        requestTimer->next = ins->next;
        ins->next = requestTimer;
    }
}


//
// Public members
//


void TimerArrayControl::attachTimer(Timer* timer){
    
    requestTimer = timer; // register timer to attach
    request = REQUEST_ATTACH;

    if (htim->Instance->CR1 & TIM_CR1_CEN && !isTickOngoing){
        // timer is running, use interrupted attach
        
        // generate interrupt to register attached timer
        htim->Instance->EGR |= TARGET_CCIG_FLAG;

        // wait for attach to happen
        // TODO: probably not necessary, can be removed if we are certain
        // when attach happened, we are done
        // while(request);

    } else {
        // timer is not running, main thread attach is safe, or we are in tick handler, interrupt attach is safe
        uint32_t cnt = __HAL_TIM_GET_COUNTER(htim);
        registerAttachedTimer(cnt);
        request = REQUEST_NONE;
    }

}

void TimerArrayControl::detachTimer(Timer* timer){
    
    requestTimer = timer; // register timer to detach
    request = REQUEST_DETACH;

    if (htim->Instance->CR1 & TIM_CR1_CEN && !isTickOngoing){
        // timer is running, use interrupted detach
        
        // generate interrupt to register detached timer
        htim->Instance->EGR |= TARGET_CCIG_FLAG;

        // wait for detach to happen
        // TODO: probably not necessary, can be removed if we are certain
        // when detach happened, we are done
        // while(request);

    } else {
        // timer is not running, main thread detach is safe
        registerDetachedTimer();
        request = REQUEST_NONE;
    }

}

void TimerArrayControl::changeTimerDelay(Timer* timer, uint32_t delay){
    
    requestTimer = timer; // register timer to change delay
    requestDelay = delay;
    request = REQUEST_DELAY_CHANGE;

    if (htim->Instance->CR1 & TIM_CR1_CEN && !isTickOngoing){
        // timer is running, use interrupted delay change
        
        // generate interrupt to register delay change timer
        htim->Instance->EGR |= TARGET_CCIG_FLAG;

        // wait for delay change to happen
        // TODO: probably not necessary, can be removed if we are certain
        // when delay change happened, we are done
        // while(request);

    } else {
        // timer is not running, main thread delay change is safe
        registerDelayChange();
        request = REQUEST_NONE;
    }
}



// -----                      -----
// ----- Timer implementation -----
// -----                      -----

Timer::Timer(const callback_function f)
    : delay(10), isPeriodic(false), f(f), running(false), next(nullptr)
{}

Timer::Timer(uint32_t delay, bool isPeriodic, const callback_function f)
    : delay(delay), isPeriodic(isPeriodic), f(f), running(false), next(nullptr)
{}

bool Timer::isRunning(){
    return running;
}
