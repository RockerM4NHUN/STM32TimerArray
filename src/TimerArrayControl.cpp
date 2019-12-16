#include "TimerArrayControl.hpp"

// capture update events and fire the timer array's callback chain
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef* htim){
    TAC_CallbackChain::fire(htim);
}

// ----- TimerArrayControl implementation -----

TimerArrayControl::TimerArrayControl(TIM_HandleTypeDef *const htim, const uint32_t fclk, const uint32_t clkdiv, const uint32_t bits) : 
    fclk(fclk),
    clkdiv(clkdiv),
    bits(bits),
    tickCallback([=](TIM_HandleTypeDef* _htim){
        if (this->htim == _htim) tick();
    }),
    htim(htim),
    timerString(Timer(nullptr))
{}

void TimerArrayControl::begin(){
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
    uint32_t target = timerString.next == nullptr ? max_count : timerString.next->target;
    __HAL_TIM_SET_COMPARE(htim, TARGET_CC_CHANNEL, target); // if no timers to fire yet, set max delay between unneeded interrupts
    HAL_TIM_OC_Start_IT(htim, TARGET_CC_CHANNEL);
}

void TimerArrayControl::tick(){
    uint32_t cnt = __HAL_TIM_GET_COUNTER(htim);
    static const auto CALLBACK_JITTER = 1000;

    while (timerString.next && ((uint32_t)(cnt - timerString.next->target)) < CALLBACK_JITTER){
        Timer* timer = timerString.next;

        // set up the next interrupt generation
        if (timer->isPeriodic){

            // set new target for timer
            timer->target += timer->period;
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
            // if timer is not periodic, it is done
            detachTimer(timerString.next);
        }

        // set the new target
        uint32_t target = timerString.next == nullptr ? max_count : timerString.next->target;
        __HAL_TIM_SET_COMPARE(htim, TARGET_CC_CHANNEL, target);

        // fire callback
        timer->f();
    }
}

void TimerArrayControl::attachTimer(Timer* timer){
    
    // if timer is already attached to a controller, do nothing
    if (timer->running) return;
    
    // get current time in ticks and add the requested delay to find the target time
    timer->target = max_count & (timer->period + __HAL_TIM_GET_COUNTER(htim));

    // find fitting place for timer in string
    Timer* it = &timerString;
    while(it->next && it->next->target < timer->target){
        // while there are more timers and the next timer's target is sooner than the new one's
        // advance it on the timer string
        it = it->next;
    }

    // insert the new timer between it and next of it
    timer->running = true;
    timer->next = it->next;
    it->next = timer;

    // if the first timer changed, adjust interrupt target
    if (timerString.next == timer) __HAL_TIM_SET_COMPARE(htim, TARGET_CC_CHANNEL, timer->target);
}

void TimerArrayControl::detachTimer(Timer* timer){
    Timer* it = &timerString;
    if (!timer->running) return;
    while(it->next && it->next != timer) it = it->next;
    if (it->next == timer){
        it->next = timer->next;
        timer->next = nullptr;
        timer->running = false;
    }
}

void TimerArrayControl::changeTimerPeriod(Timer* timer, uint32_t period){
    
    if (!timer->running) {
        timer->period = period;
        return;
    }

    timer->target += period - timer->period;
    timer->target &= max_count; 
    timer->period = period;
    
    // find fitting place for timer in string
    Timer* ins = &timerString;
    Timer* rem = ins;
    while(ins->next && ins->next->target < timer->target){
        // while there are more timers and the next timer's target is sooner than the modified one's
        // advance |ins| on the timer string
        ins = ins->next;

        // if the next timer is not our's to remove, advance |rem| on the string
        if (rem->next != timer) rem = ins;
    }
    while(rem->next && rem->next != timer) rem = rem->next;
    
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
        rem->next = timer->next;

        // insert our timer between |ins| and next of |ins|
        timer->next = ins->next;
        ins->next = timer;
    }
}



// ----- Timer implementation -----

Timer::Timer(const callback_function f)
    : period(10), isPeriodic(false), f(f), running(false), next(nullptr)
{}

Timer::Timer(uint32_t period, bool isPeriodic, const callback_function f)
    : period(period), isPeriodic(isPeriodic), f(f), running(false), next(nullptr)
{}

bool Timer::isRunning(){
    return running;
}
