#include "TimerArrayControl.hpp"

// capture update events and fire the timer array's callback chain
// a single call to tick would suffice in case of one timer array,
// but this way multiple callback handlers for the same interrupt
// routine can exist independently, without requiring rewriting
// the function for the current setup at all times
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef* htim){
    TAC_CallbackChain::fire(htim);
}

#define __HAL_IS_TIMER_ENABLED(htim) (htim->Instance->CR1 & TIM_CR1_CEN)
#define __HAL_GENERATE_INTERRUPT(htim, EGR_FLAG) (htim->Instance->EGR |= EGR_FLAG)

// -----                                  -----
// ----- TimerArrayControl implementation -----
// -----                                  -----


TimerArrayControl::TimerArrayControl(TIM_HandleTypeDef *const htim, const uint32_t fclk, const uint32_t clkdiv, const uint32_t bits) : 
    fclk(fclk),
    clkdiv(clkdiv),
    bits(bits),
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

void TimerArrayControl::stop(){
    // stop timer if it was running
    HAL_TIM_OC_Stop_IT(htim, TARGET_CC_CHANNEL);
}

void TimerArrayControl::f(TIM_HandleTypeDef* _htim){
    if (this->htim == _htim) tick();
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
        case REQUEST_DELAY_CHANGE: registerDelayChange(cnt); break;
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
        timer->fire();

        // refetch the counter, could have changed significantly since the tick's start
        cnt = __HAL_TIM_GET_COUNTER(htim);
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

void TimerArrayControl::registerDelayChange(uint32_t cnt){

    if (!requestTimer->running) {
        requestTimer->delay = requestDelay;
        return;
    }

    // if there are not more ticks until the current target is reached then
    // the ticks we try to bring the target early, the new target will be in tha past
    if ((uint32_t)(max_count & (requestTimer->target - cnt)) <= requestTimer->delay - requestDelay){
        // the requested delay is already passed, skip the callbacks between the start and now
        
        // add the requested delay until the target is in the future
        // arithmetic magic is necessary for correct handling of both 16 and 32 bit counters
        requestTimer->target -= requestTimer->delay;
        do {
            requestTimer->target += requestDelay;
            requestTimer->target &= max_count;
        } while ((uint32_t)(max_count & (cnt - requestTimer->target)) < (max_count >> 1));
        requestTimer->delay = requestDelay;

    } else {
        // the new target will be in the future, set it as usual
        requestTimer->target += requestDelay - requestTimer->delay;
        requestTimer->target &= max_count;
        requestTimer->delay = requestDelay;
    }

    
    // find fitting place for timer in string
    Timer* ins = &timerString;
    Timer* rem = ins;

    // search attach position
    while(ins->next && ins->next->target < requestTimer->target){
        // while there are more timers and the next timer's target is sooner than the modified one's
        // advance |ins| on the timer string
        ins = ins->next;

        // if the next timer is not our's to remove, advance |rem| on the string
        if (rem->next != requestTimer) rem = ins;
    }

    // search where the timer was, to detach it from that position
    while(rem->next && rem->next != requestTimer) rem = rem->next;
    
    // If the interrupt was set to a timer that has changed, set new target.
    // If ins is first timer, the timer was put to first place.
    // If rem is first timer, the timer was moved from first place.
    // If both, the first timers target was probably changed.
    // In all cases new target is needed.
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

void TimerArrayControl::registerAttachedTimerInSync(uint32_t cnt){

    // won't reattach timer (if attached to this controller, it would be possible)
    if (requestTimer->running) return;

    requestTimer->target = max_count & (requestReferenceTimer->target - requestReferenceTimer->delay);
    
    // add the requested delay until the target is in the future
    // arithmetic magic is necessary for correct handling of both 16 and 32 bit counters
    do {
        requestTimer->target += requestTimer->delay;
        requestTimer->target &= max_count;
    } while ((uint32_t)(max_count & (cnt - requestTimer->target)) < (max_count >> 1));

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


//
// Public members
//


void TimerArrayControl::attachTimer(Timer* timer){
    
    requestTimer = timer; // register timer to attach
    request = REQUEST_ATTACH;

    if (__HAL_IS_TIMER_ENABLED(htim) && !isTickOngoing){
        // timer is running, use interrupted attach
        
        // generate interrupt to register attached timer
        __HAL_GENERATE_INTERRUPT(htim, TARGET_CCIG_FLAG);

        // don't wait for attach to happen, the interrupt will handle it very quickly
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

    if (__HAL_IS_TIMER_ENABLED(htim) && !isTickOngoing){
        // timer is running, use interrupted detach
        
        // generate interrupt to register detached timer
        __HAL_GENERATE_INTERRUPT(htim, TARGET_CCIG_FLAG);

        // don't wait for detach to happen, the interrupt will handle it very quickly
    } else {
        // timer is not running, main thread detach is safe
        registerDetachedTimer();
        request = REQUEST_NONE;
    }

}

void TimerArrayControl::changeTimerDelay(Timer* timer, uint32_t delay){

    // can't handle 0 delay
    if (delay == 0) return;
    
    requestTimer = timer; // register timer to change delay
    requestDelay = delay;
    request = REQUEST_DELAY_CHANGE;

    if (__HAL_IS_TIMER_ENABLED(htim) && !isTickOngoing){
        // timer is running, use interrupted delay change
        
        // generate interrupt to register delay change timer
        __HAL_GENERATE_INTERRUPT(htim, TARGET_CCIG_FLAG);

        // don't wait for delay change to happen, the interrupt will handle it very quickly
    } else {
        // timer is not running, main thread delay change is safe
        uint32_t cnt = __HAL_TIM_GET_COUNTER(htim);
        registerDelayChange(cnt);
        request = REQUEST_NONE;
    }
}

void TimerArrayControl::attachTimerInSync(Timer* timer, Timer* reference){
    
    // won't reattach timer (if attached to this controller, it would be possible)
    if (timer->running) return;

    requestTimer = timer; // register timer to attach
    requestReferenceTimer = reference;
    request = REQUEST_ATTACH_SYNC;

    if (__HAL_IS_TIMER_ENABLED(htim) && !isTickOngoing){
        // timer is running, use interrupted attach
        
        // generate interrupt to register attached timer
        __HAL_GENERATE_INTERRUPT(htim, TARGET_CCIG_FLAG);

        // don't wait for attach to happen, the interrupt will handle it very quickly
    } else {
        // timer is not running, main thread attach is safe, or we are in tick handler, interrupt attach is safe
        uint32_t cnt = __HAL_TIM_GET_COUNTER(htim);
        registerAttachedTimerInSync(cnt);
        request = REQUEST_NONE;
    }
}



// -----                      -----
// ----- Timer implementation -----
// -----                      -----

Timer::Timer(const callback_function f)
    : delay(10), isPeriodic(false), f((void*)f), running(false), next(nullptr)
{}

Timer::Timer(uint32_t delay, bool isPeriodic, const callback_function f)
    : delay(delay), isPeriodic(isPeriodic), f((void*)f), running(false), next(nullptr)
{}

bool Timer::isRunning(){
    return running;
}

void Timer::fire(){
    ((callback_function)f)();
}