#include "TimerArrayControl.hpp"

// capture update events and fire the timer array's callback chain
// a single call to tick would suffice in case of one timer array,
// but this way multiple callback handlers for the same interrupt
// routine can exist independently, without requiring rewriting
// the function for the current setup at all times
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef* htim){
    TIM_OC_DelayElapsed_CallbackChain::fire(htim);
}

#define __HAL_IS_TIMER_ENABLED(htim) (htim->Instance->CR1 & TIM_CR1_CEN)
#define __HAL_GENERATE_INTERRUPT(htim, EGR_FLAG) (htim->Instance->EGR |= EGR_FLAG)
#define COUNTER_MODULO(x) (max_count & ((uint32_t)(x)))


// -----                            -----
// ----- TimerString implementation -----
// -----                            -----

TimerArrayControl::TimerFeed::TimerFeed(TIM_HandleTypeDef *const htim) : root(nullptr), htim(htim) {}

Timer* TimerArrayControl::TimerFeed::findTimerInsertionLink(Timer* it, Timer* timer){
    while(it->next && it->next->target < timer->target){
        // while there are more timers and the next timer's target is sooner than the new one's
        // advance it on the timer string
        it = it->next;
    }
    return it;
}

Timer* TimerArrayControl::TimerFeed::findTimerInsertionLink(Timer* timer){
    return findTimerInsertionLink(&root, timer);
}

// insert timer after the iterator
void TimerArrayControl::TimerFeed::insertTimer(Timer* it, Timer* timer){
    
    // insert the new timer between it and next of it
    timer->running = true;
    timer->next = it->next;
    it->next = timer;

    // if the first timer changed, adjust interrupt target
    if (root.next == timer) __HAL_TIM_SET_COMPARE(htim, TARGET_CC_CHANNEL, timer->target);
}

// insert timer based on target
void TimerArrayControl::TimerFeed::insertTimer(Timer* timer){
    insertTimer(findTimerInsertionLink(&root, timer), timer);
}

// remove timer from feed
void TimerArrayControl::TimerFeed::removeTimer(Timer* timer){
    Timer* it = &root;
    while(it->next && it->next != timer) it = it->next;

    if (it->next == timer){
        it->next = timer->next;
        timer->next = nullptr;
        timer->running = false;
    }

    // if the removed timer was the first in the feed, update interrupt target
    if (&root == it && root.next) __HAL_TIM_SET_COMPARE(htim, TARGET_CC_CHANNEL, root.next->target);
}

// remove and insert timer in one operation, according to it's target
void TimerArrayControl::TimerFeed::updateTarget(Timer* timer, uint32_t target){
    
    // find fitting place for timer in string
    Timer* ins = &root;
    Timer* rem = ins;

    // search attach position
    while(ins->next && ins->next->target < target){
        // while there are more timers and the next timer's target is sooner than the modified one's
        // advance |ins| on the timer string
        ins = ins->next;

        // if the next timer is not our's to remove, advance |rem| on the string
        if (rem->next != timer) rem = ins;
    }

    // search where the timer was, to detach it from that position
    while(rem->next && rem->next != timer) rem = rem->next;

    // only move timer if the predecessor changed and it is not itself
    if (ins != rem && ins != timer){
        // remove our timer from the string
        rem->next = timer->next;

        // insert our timer between |ins| and next of |ins|
        timer->next = ins->next;
        ins->next = timer;
    }

    // update the timer's target
    timer->target = target;
    
    // If the interrupt was set to a timer that has changed, set new target.
    // If ins is first timer, the timer was put to first place.
    // If rem is first timer, the timer was moved from first place.
    // If both, the first timers target was probably changed.
    // In all cases new target is needed.
    if (&root == ins || &root == rem) {
        __HAL_TIM_SET_COMPARE(htim, TARGET_CC_CHANNEL, root.next->target);
    }
}

// -----                                  -----
// ----- TimerArrayControl implementation -----
// -----                                  -----


TimerArrayControl::TimerArrayControl(TIM_HandleTypeDef *const htim, const uint32_t fclk, const uint32_t clkdiv, const uint32_t bits) : 
    fclk(fclk),
    clkdiv(clkdiv),
    bits(bits),
    timerFeed(htim),
    request(NONE),
    requestTimer(nullptr),
    requestDelay(0),
    isTickOngoing(false)
{}

void TimerArrayControl::begin(){

    // stop timer if it was running
    HAL_TIM_OC_Stop_IT(timerFeed.htim, TARGET_CC_CHANNEL);

    timerFeed.htim->Init.CounterMode = TIM_COUNTERMODE_UP; // all STM32 counters support it
    timerFeed.htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE; // by disabling, write to ARR shadow regs happens immedietely
    timerFeed.htim->Init.Period = max_count; // set max period for maximum amount of possible delay
    timerFeed.htim->Init.Prescaler = prescaler - 1; // prescaler divides clock by Prescaler+1

    TIM_OC_InitTypeDef oc_init;
    oc_init.OCMode = TIM_OCMODE_TIMING;

    HAL_TIM_OC_Init(timerFeed.htim);
    HAL_TIM_OC_ConfigChannel(timerFeed.htim, &oc_init, TARGET_CC_CHANNEL);
    uint32_t cnt = __HAL_TIM_GET_COUNTER(timerFeed.htim);
    uint32_t target = timerFeed.root.next == nullptr ? (max_count & (cnt-1)) : timerFeed.root.next->target;
    __HAL_TIM_SET_COMPARE(timerFeed.htim, TARGET_CC_CHANNEL, target); // if no timers to fire yet, set max delay between unneeded interrupts
    HAL_TIM_OC_Start_IT(timerFeed.htim, TARGET_CC_CHANNEL);
}

void TimerArrayControl::stop(){
    // stop timer if it was running
    HAL_TIM_OC_Stop_IT(timerFeed.htim, TARGET_CC_CHANNEL);
}

void TimerArrayControl::f(TIM_HandleTypeDef* htim){
    if (timerFeed.htim == htim) tick();
}

/**
 * This method can only be called from interupts.
 * */
void TimerArrayControl::tick(){

    isTickOngoing = true;

    uint32_t cnt = __HAL_TIM_GET_COUNTER(timerFeed.htim);
    static const auto CALLBACK_JITTER = 1000;

    // handle request
    switch(request){
        case ATTACH: registerAttachedTimer(cnt, requestTimer); break;
        case DETACH: registerDetachedTimer(requestTimer); break;
        case DELAY_CHANGE: registerDelayChange(cnt, requestTimer, requestDelay); break;
        case ATTACH_SYNC: registerAttachedTimerInSync(cnt, requestTimer, requestReferenceTimer); break;
        case MANUAL_FIRE: registerManualFire(cnt, requestTimer); break;
        default: break;
    }
    request = NONE;

    // handle timeout
    while (timerFeed.root.next && ((uint32_t)(cnt - timerFeed.root.next->target)) < CALLBACK_JITTER){
        Timer* timer = timerFeed.root.next;

        // set up the next interrupt generation
        if (timer->periodic){

            // set new target for timer
            uint32_t target = timer->target;
            target += timer->delay;
            target &= max_count;

            // find fitting place for timer in string
            timerFeed.updateTarget(timer, target);

        } else {
            // if timer is not periodic, it is done, we can detach it
            Timer* timer = timerFeed.root.next;
            timer->running = false;
            timerFeed.root.next = timer->next;
            timer->next = nullptr;
        }

        // set the new target
        uint32_t target = timerFeed.root.next == nullptr ? (max_count & (cnt - 1)) : timerFeed.root.next->target;
        __HAL_TIM_SET_COMPARE(timerFeed.htim, TARGET_CC_CHANNEL, target);

        // fire callback
        timer->fire();

        // refetch the counter, could have changed significantly since the tick's start
        cnt = __HAL_TIM_GET_COUNTER(timerFeed.htim);
    }

    isTickOngoing = false;
}

void TimerArrayControl::registerAttachedTimer(uint32_t cnt, Timer* timer){

    // if timer is already attached to a controller, do nothing
    if (timer->running) return;

    // get current time in ticks and add the requested delay to find the target time
    timer->target = max_count & (timer->delay + cnt);

    // insert timer based on the target time
    timerFeed.insertTimer(timer);
}

void TimerArrayControl::registerDetachedTimer(Timer* timer){
    if (!timer->running) return;
    timerFeed.removeTimer(timer);
}

void TimerArrayControl::registerDelayChange(uint32_t cnt, Timer* timer, uint32_t delay){

    if (!timer->running) {
        timer->delay = delay;
        return;
    }

    // TODO: negative calculation might be also needed, for more complicated cases
    // calculate start time of timer, then the next firing time, skipping already passed opportunities
    uint32_t target = COUNTER_MODULO(timer->target - timer->delay);
    target = calculateNextFireInSync(target, cnt, delay);

    timer->delay = delay;

    // update the position of timer in the feed
    timerFeed.updateTarget(timer, target);
}

void TimerArrayControl::registerAttachedTimerInSync(uint32_t cnt, Timer* timer, Timer* reference){

    // won't reattach timer (if attached to this controller, it would be possible)
    if (timer->running) return;

    // TODO: negative calculation might be also needed, for more complicated cases
    // put start time in timer's target, find the next firing time with timer's delay
    timer->target = COUNTER_MODULO(reference->target - reference->delay);
    timer->target = calculateNextFireInSync(timer->target, cnt, timer->delay);

    // find fitting place for timer in string
    timerFeed.insertTimer(timer);
}

void TimerArrayControl::registerManualFire(uint32_t cnt, Timer* timer){

    // fire timer manually, even if it is not running
    // firing a periodic timer will start it
    timer->fire();
    
    // if timer was running detach it, if it was periodic it will be immedietely reattached
    if (timer->running) timerFeed.removeTimer(timer);

    // if timer is periodic, restart it at this moment
    if (timer->periodic){
        timer->running = false;
        registerAttachedTimer(cnt, timer);
    }
}


//
// Public members
//


void TimerArrayControl::attachTimer(Timer* timer){
    
    requestTimer = timer; // register timer to attach

    if (__HAL_IS_TIMER_ENABLED(timerFeed.htim) && !isTickOngoing){
        // timer is running, use interrupted attach

        request = ATTACH;
        
        // generate interrupt to register attached timer
        __HAL_GENERATE_INTERRUPT(timerFeed.htim, TARGET_CCIG_FLAG);

        // don't wait for attach to happen, the interrupt will handle it very quickly
    } else {
        // timer is not running, main thread attach is safe, or we are in tick handler, interrupt attach is safe
        uint32_t cnt = __HAL_TIM_GET_COUNTER(timerFeed.htim);
        registerAttachedTimer(cnt, timer);
    }

}

void TimerArrayControl::detachTimer(Timer* timer){
    
    requestTimer = timer; // register timer to detach

    if (__HAL_IS_TIMER_ENABLED(timerFeed.htim) && !isTickOngoing){
        // timer is running, use interrupted detach

        request = DETACH;
        
        // generate interrupt to register detached timer
        __HAL_GENERATE_INTERRUPT(timerFeed.htim, TARGET_CCIG_FLAG);

        // don't wait for detach to happen, the interrupt will handle it very quickly
    } else {
        // timer is not running, main thread detach is safe
        registerDetachedTimer(timer);
    }

}

void TimerArrayControl::changeTimerDelay(Timer* timer, uint32_t delay){

    // can't handle 0 delay
    if (delay == 0) return;
    
    requestTimer = timer; // register timer to change delay
    requestDelay = delay;

    if (__HAL_IS_TIMER_ENABLED(timerFeed.htim) && !isTickOngoing){
        // timer is running, use interrupted delay change
        
        request = DELAY_CHANGE;

        // generate interrupt to register delay change timer
        __HAL_GENERATE_INTERRUPT(timerFeed.htim, TARGET_CCIG_FLAG);

        // don't wait for delay change to happen, the interrupt will handle it very quickly
    } else {
        // timer is not running, main thread delay change is safe
        uint32_t cnt = __HAL_TIM_GET_COUNTER(timerFeed.htim);
        registerDelayChange(cnt, timer, delay);
    }
}

void TimerArrayControl::attachTimerInSync(Timer* timer, Timer* reference){
    
    // won't reattach timer (if attached to this controller, it would be possible)
    if (timer->running) return;

    requestTimer = timer; // register timer to attach
    requestReferenceTimer = reference;

    if (__HAL_IS_TIMER_ENABLED(timerFeed.htim) && !isTickOngoing){
        // timer is running, use interrupted attach

        request = ATTACH_SYNC;
        
        // generate interrupt to register attached timer
        __HAL_GENERATE_INTERRUPT(timerFeed.htim, TARGET_CCIG_FLAG);

        // don't wait for attach to happen, the interrupt will handle it very quickly
    } else {
        // timer is not running, main thread attach is safe, or we are in tick handler, interrupt attach is safe
        uint32_t cnt = __HAL_TIM_GET_COUNTER(timerFeed.htim);
        registerAttachedTimerInSync(cnt, timer, reference);
    }
}

void TimerArrayControl::manualFire(Timer* timer){
    
    requestTimer = timer; // register timer to manually fire

    if (__HAL_IS_TIMER_ENABLED(timerFeed.htim) && !isTickOngoing){
        // timer is running, use interrupted manual fire

        request = MANUAL_FIRE;
        
        // generate interrupt to register manual fire 
        __HAL_GENERATE_INTERRUPT(timerFeed.htim, TARGET_CCIG_FLAG);

        // don't wait for it to happen, the interrupt will handle it very quickly
    } else {
        // timer is not running, main thread attach is safe, or we are in tick handler, interrupt attach is safe
        uint32_t cnt = __HAL_TIM_GET_COUNTER(timerFeed.htim);
        registerManualFire(cnt, timer);
    }
}

uint32_t TimerArrayControl::remainingTicks(Timer* timer) const {
    if (!timer->running) return 0;
    const uint32_t cnt = __HAL_TIM_GET_COUNTER(timerFeed.htim);
    return max_count & (timer->target - cnt);
}

uint32_t TimerArrayControl::elapsedTicks(Timer* timer) const {
    if (!timer->running) return 0;
    return timer->delay - remainingTicks(timer);
}

float TimerArrayControl::actualTickFrequency() const {
    return ((float)fclk)/prescaler;
}

uint32_t TimerArrayControl::calculateNextFireInSync(uint32_t target, uint32_t cnt, uint32_t delay) const{
    uint32_t diff = COUNTER_MODULO(cnt - target);
    uint32_t subt = diff - (diff/delay)*delay;
    uint32_t incr = delay - subt;
    uint32_t tnext = COUNTER_MODULO(cnt + incr);
    return tnext;
}


// -----                      -----
// ----- Timer implementation -----
// -----                      -----

Timer::Timer(const callback_function f)
    : delay(10), periodic(false), f((void*)f), running(false), next(nullptr)
{}

Timer::Timer(uint32_t delay, bool isPeriodic, const callback_function f)
    : delay(delay), periodic(isPeriodic), f((void*)f), running(false), next(nullptr)
{}

bool Timer::isRunning(){
    return running;
}

void Timer::fire(){
    ((callback_function)f)();
}