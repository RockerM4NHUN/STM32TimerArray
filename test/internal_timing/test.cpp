#include "../hwsetup.h"
#include <unity.h>

// A bit ugly, but effective way to enable full control while testing.
// In C++ I don't know any other way to allow access of private and
// protected members while testing, without polluting the source itself.
#define protected public
 #define private  public
  #include "TimerArrayControl.hpp"
 #undef private
#undef protected

/*
 * Test features that require a configured timer periphery and test simple
 * timing functionalities of TimerArrayControl, by comparing the timing of
 * SysTick and the library implementation. The result of some tests can 
 * depend on the fcnt frequency, 10 kHz is safe, 100 kHz is usually safe.
 */

// Test helpers, might be transferred to hwsetup
static const auto TIM_IT_FLAG = TIM_IT_CC1;
static const uint32_t fclk = 72'000'000;
static const uint32_t fcnt = 100'000; // above 100 kHz test numbers can be off by a few
static const uint16_t clkdiv = fclk / fcnt;
static TimerArrayControl* hcontrol = nullptr;
static bool timerFiredFlag;
void timerFiredCallback(){ timerFiredFlag = true; }
volatile uint8_t rwDummy;
uint8_t idleOnDummy(){ return rwDummy; }

// -----                                       -----
// ----- Test callback and interrupt mechanism -----
// -----                                       -----

struct TestCallbackChainFire : TIM_OC_DelayElapsed_CallbackChain{
    TIM_HandleTypeDef** handle;
    uint8_t* hflag;
    TestCallbackChainFire(TIM_HandleTypeDef** _handle, uint8_t* _hflag) : handle(_handle), hflag(_hflag) {}
    void chainedCallback(TIM_HandleTypeDef* htim){ *handle = htim; *hflag = 1; }
};
void test_callback_chain_ctor_and_dtor(){
    TIM_HandleTypeDef* testHandle = 0;
    uint8_t testFlag = 0;

    {
        TestCallbackChainFire obj(&testHandle, &testFlag);
        
        if (testFlag) TEST_FAIL_MESSAGE("Callback fired prematurely");
        HAL_TIM_OC_DelayElapsedCallback(&htim2);
        if (!testFlag) TEST_FAIL_MESSAGE("Callback not fired");

        TEST_ASSERT_EQUAL_PTR(&htim2, testHandle);
    }

    testFlag = 0;
    HAL_TIM_OC_DelayElapsedCallback(&htim2);
    if (testFlag) TEST_FAIL_MESSAGE("Callback fired after link is destroyed");
}

void test_interrupt_blocking_of_request(){
    // if this test fails, all registration tests should fail too
    // passes if the interrupt generated through TimerArrayControl is blocked

    TimerArrayControl& control = *hcontrol;

    TEST_ASSERT_FALSE(control.timerFeed.htim->Instance->CR1 & TIM_CR1_CEN); // timer was disabled
    control.begin(); // enables timer interrupt
    TEST_ASSERT_TRUE(control.timerFeed.htim->Instance->CR1 & TIM_CR1_CEN); // timer is enabled
    TEST_ASSERT_TRUE(__HAL_TIM_GET_IT_SOURCE(control.timerFeed.htim, TIM_IT_FLAG)); // timer interrupt was enabled
    __HAL_TIM_DISABLE_IT(control.timerFeed.htim, TIM_IT_FLAG); // disable timer interrupt, to prevent tick
    
    // set interrupt to near future, so the flag will be set, when re-enabled the interrupt will happen
    control.timerFeed.htim->Instance->CCR1 = 2 + __HAL_TIM_GET_COUNTER(control.timerFeed.htim);

    {
        TIM_HandleTypeDef* testHandle = 0;
        volatile uint8_t testFlag = 0;
        TestCallbackChainFire obj(&testHandle, (uint8_t*)&testFlag); // register callback handler

        HAL_Delay(10); // add some cycles for interrupt to kick in (10 ms to be really sure)
        TEST_ASSERT_EQUAL(0, testFlag); // test if callback was fired
        
        // test for reintroduction of the cached interrupt
        __HAL_TIM_ENABLE_IT(control.timerFeed.htim, TIM_IT_FLAG); // enable timer interrupt, to generate callback
        HAL_Delay(0); // add some CPU cycles for interrupt to kick in
        TEST_ASSERT_EQUAL(1, testFlag); // test if callback was fired
    }
}

// -----                               -----
// ----- Test API request registration -----
// -----                               -----

void test_attach_request_registration(){
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);

    control.begin();
    __HAL_TIM_DISABLE_IT(&htim2, TIM_IT_FLAG); // disable timer interrupt, to prevent tick

    control.attachTimer(&timer);
    TEST_ASSERT_EQUAL(TimerArrayControl::ATTACH, control.request);
    TEST_ASSERT_EQUAL_PTR(&timer, control.requestTimer);
}

void test_detach_request_registration(){
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);

    control.begin();
    __HAL_TIM_DISABLE_IT(&htim2, TIM_IT_FLAG); // disable timer interrupt, to prevent tick

    control.detachTimer(&timer);
    TEST_ASSERT_EQUAL(TimerArrayControl::DETACH, control.request);
    TEST_ASSERT_EQUAL_PTR(&timer, control.requestTimer);
}

void test_delay_change_request_registration(){
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);

    control.begin();
    __HAL_TIM_DISABLE_IT(&htim2, TIM_IT_FLAG); // disable timer interrupt, to prevent tick

    control.changeTimerDelay(&timer, 123);
    TEST_ASSERT_EQUAL(TimerArrayControl::DELAY_CHANGE, control.request);
    TEST_ASSERT_EQUAL_PTR(&timer, control.requestTimer);
    TEST_ASSERT_EQUAL(123, control.requestDelay);
}

void test_attach_in_sync_request_registration(){
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);
    Timer reftimer(100, false, timerFiredCallback);

    control.begin();
    __HAL_TIM_DISABLE_IT(&htim2, TIM_IT_FLAG); // disable timer interrupt, to prevent tick

    control.attachTimerInSync(&timer, &reftimer);
    TEST_ASSERT_EQUAL(TimerArrayControl::ATTACH_SYNC, control.request);
    TEST_ASSERT_EQUAL_PTR(&timer, control.requestTimer);
    TEST_ASSERT_EQUAL_PTR(&reftimer, control.requestReferenceTimer);
}

void test_manual_fire_request_registration(){
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);

    control.begin();
    __HAL_TIM_DISABLE_IT(&htim2, TIM_IT_FLAG); // disable timer interrupt, to prevent tick

    control.manualFire(&timer);
    TEST_ASSERT_EQUAL(TimerArrayControl::MANUAL_FIRE, control.request);
    TEST_ASSERT_EQUAL_PTR(&timer, control.requestTimer);
}

// -----                                             -----
// ----- Test API request handling from direct calls -----
// -----                                             -----

void test_attach_timer_from_direct_call_01(){
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);

    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 234);

    control.attachTimer(&timer);
    TEST_ASSERT_TRUE(timer.running); // timer was attached and now running
    TEST_ASSERT_EQUAL(334, timer.target); // the target is the value of the counter plus the set delay
    TEST_ASSERT_EQUAL_PTR(&timer, control.timerFeed.root.next); // timer was put into timerFeed
    TEST_ASSERT_EQUAL_PTR(0, control.timerFeed.root.next->next); // the timer was put in only once
}

void test_detach_timer_from_direct_call_01(){
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);

    control.attachTimer(&timer); // attach before detach

    control.detachTimer(&timer);
    TEST_ASSERT_FALSE(timer.running); // timer was detached and is not running
    TEST_ASSERT_EQUAL_PTR(0, control.timerFeed.root.next); // timer was removed from timerFeed
    TEST_ASSERT_EQUAL_PTR(0, timer.next); // the timer was detached properly
}

void test_delay_change_from_direct_call_01(){
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);

    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 200);
    control.attachTimer(&timer); // attach before delay change
    // target should be 300

    control.changeTimerDelay(&timer, 123);
    TEST_ASSERT_EQUAL(123, timer.delay()); // the delay was updated
    TEST_ASSERT_EQUAL(323, timer.target); // extending the timer by 23 ticks delays the original target
    TEST_ASSERT_EQUAL_PTR(&timer, control.timerFeed.root.next); // the timer is still attached
    TEST_ASSERT_EQUAL_PTR(0, control.timerFeed.root.next->next); // there is only one timer attached
}

void test_attach_in_sync_from_direct_call_01(){
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);
    Timer reftimer(1000, false, timerFiredCallback);

    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 200);
    control.attachTimer(&reftimer); // attach reference

    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 270); // change time of second attach
    control.attachTimerInSync(&timer, &reftimer);
    TEST_ASSERT_TRUE(timer.running); // the timer is running
    TEST_ASSERT_TRUE(reftimer.running); // the reference timer is running
    TEST_ASSERT_EQUAL(300, timer.target); // despite the current time, timer is started at 200, hence target is 300
    TEST_ASSERT_EQUAL(1200, reftimer.target); // the reference timer's target did not change
    TEST_ASSERT_EQUAL_PTR(&timer, control.timerFeed.root.next); // |timer| is first in feed
    TEST_ASSERT_EQUAL_PTR(&reftimer, timer.next); // |reftimer| is second in feed
    TEST_ASSERT_EQUAL_PTR(0, reftimer.next); // no more timers in feed
}

void test_manual_fire_from_direct_call_01(){
    // non attached, aperiodic timer
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);

    TEST_ASSERT_FALSE(timerFiredFlag); // no premature fire
    control.manualFire(&timer);
    TEST_ASSERT_TRUE(timerFiredFlag); // timer fired as expected
    TEST_ASSERT_FALSE(timer.running); // timer is one shot, no attach needed
    TEST_ASSERT_EQUAL_PTR(0, control.timerFeed.root.next); // timer is not attached, for real
}

void test_manual_fire_from_direct_call_02(){
    // non attached, periodic timer
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, true, timerFiredCallback);

    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 20); // time when manual fire happens

    TEST_ASSERT_FALSE(timerFiredFlag); // no premature fire
    control.manualFire(&timer);
    TEST_ASSERT_TRUE(timerFiredFlag); // timer fired as expected
    TEST_ASSERT_TRUE(timer.running); // timer is periodic, attach is needed
    TEST_ASSERT_EQUAL(120, timer.target); // target was adjusted
    TEST_ASSERT_EQUAL_PTR(&timer, control.timerFeed.root.next); // timer is attached, for real
    TEST_ASSERT_EQUAL_PTR(0, timer.next); // timer is attached only once
}

void test_manual_fire_from_direct_call_03(){
    // attached, aperiodic timer
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);

    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 20);
    control.attachTimer(&timer);

    TEST_ASSERT_FALSE(timerFiredFlag); // no premature fire
    control.manualFire(&timer);
    TEST_ASSERT_TRUE(timerFiredFlag); // timer fired as expected
    TEST_ASSERT_FALSE(timer.running); // timer is one shot, it was removed after firing
    TEST_ASSERT_EQUAL_PTR(0, control.timerFeed.root.next); // timer is not attached, for real
}

void test_manual_fire_from_direct_call_04(){
    // attached, periodic timer
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, true, timerFiredCallback);

    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 20);
    control.attachTimer(&timer);
    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 50); // time when manual fire happens

    TEST_ASSERT_FALSE(timerFiredFlag); // no premature fire
    control.manualFire(&timer);
    TEST_ASSERT_TRUE(timerFiredFlag); // timer fired as expected
    TEST_ASSERT_TRUE(timer.running); // timer is periodic, reattach is needed
    TEST_ASSERT_EQUAL(150, timer.target); // target was adjusted, according to time when fired
    TEST_ASSERT_EQUAL_PTR(&timer, control.timerFeed.root.next); // timer is attached, for real
    TEST_ASSERT_EQUAL_PTR(0, timer.next); // timer is attached only once
}

// -----                                                -----
// ----- Test API request handling from interrupt calls -----
// -----                                                -----

void test_attach_timer_from_interrupt_call_01(){
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);

    control.begin();

    // set cnt, to have a non-trivial target time for timer
    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 234);

    control.attachTimer(&timer);
    idleOnDummy(); // dummy write, to give some time to interrupt generation before testing results
    
    TEST_ASSERT_TRUE(timer.running); // timer was attached and now running
    TEST_ASSERT_EQUAL(334, timer.target); // the target is the value of the counter plus the set delay
    TEST_ASSERT_EQUAL_PTR(&timer, control.timerFeed.root.next); // timer was put into timerFeed
    TEST_ASSERT_EQUAL_PTR(0, control.timerFeed.root.next->next); // the timer was put in only once
}

void test_detach_timer_from_interrupt_call_01(){
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);

    control.begin();
    control.attachTimer(&timer); // attach before detach

    control.detachTimer(&timer);
    idleOnDummy(); // dummy write, to give some time to interrupt generation before testing results
    
    TEST_ASSERT_FALSE(timer.running); // timer was detached and is not running
    TEST_ASSERT_EQUAL_PTR(0, control.timerFeed.root.next); // timer was removed from timerFeed
    TEST_ASSERT_EQUAL_PTR(0, timer.next); // the timer was detached properly
}

void test_delay_change_from_interrupt_call_01(){
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);

    control.begin();
    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 200);
    control.attachTimer(&timer); // attach before delay change
    // target should be 300

    control.changeTimerDelay(&timer, 123);
    idleOnDummy(); // dummy write, to give some time to interrupt generation before testing results
    
    TEST_ASSERT_EQUAL(123, timer.delay()); // the delay was updated
    TEST_ASSERT_EQUAL(323, timer.target); // extending the timer by 23 ticks delays the original target
    TEST_ASSERT_EQUAL_PTR(&timer, control.timerFeed.root.next); // the timer is still attached
    TEST_ASSERT_EQUAL_PTR(0, control.timerFeed.root.next->next); // there is only one timer attached
}

void test_attach_in_sync_from_interrupt_call_01(){
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);
    Timer reftimer(1000, false, timerFiredCallback);

    control.begin();
    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 200);
    control.attachTimer(&reftimer); // attach reference

    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 270); // change time of second attach
    control.attachTimerInSync(&timer, &reftimer);
    idleOnDummy(); // dummy write, to give some time to interrupt generation before testing results
    
    TEST_ASSERT_TRUE(timer.running); // the timer is running
    TEST_ASSERT_TRUE(reftimer.running); // the reference timer is running
    TEST_ASSERT_EQUAL(300, timer.target); // despite the current time, timer is started at 200, hence target is 300
    TEST_ASSERT_EQUAL(1200, reftimer.target); // the reference timer's target did not change
    TEST_ASSERT_EQUAL_PTR(&timer, control.timerFeed.root.next); // |timer| is first in feed
    TEST_ASSERT_EQUAL_PTR(&reftimer, timer.next); // |reftimer| is second in feed
    TEST_ASSERT_EQUAL_PTR(0, reftimer.next); // no more timers in feed
}

void test_manual_fire_from_interrupt_call_01(){
    // non attached, aperiodic timer
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);

    control.begin();
    TEST_ASSERT_FALSE(timerFiredFlag); // no premature fire
    control.manualFire(&timer);
    idleOnDummy(); // dummy write, to give some time to interrupt generation before testing results
    
    TEST_ASSERT_TRUE(timerFiredFlag); // timer fired as expected
    TEST_ASSERT_FALSE(timer.running); // timer is one shot, no attach needed
    TEST_ASSERT_EQUAL_PTR(0, control.timerFeed.root.next); // timer is not attached, for real
}

void test_manual_fire_from_interrupt_call_02(){
    // non attached, periodic timer
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, true, timerFiredCallback);

    control.begin();
    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 20); // time when manual fire happens

    TEST_ASSERT_FALSE(timerFiredFlag); // no premature fire
    control.manualFire(&timer);
    idleOnDummy(); // dummy write, to give some time to interrupt generation before testing results
    
    TEST_ASSERT_TRUE(timerFiredFlag); // timer fired as expected
    TEST_ASSERT_TRUE(timer.running); // timer is periodic, attach is needed
    TEST_ASSERT_EQUAL(120, timer.target); // target was adjusted
    TEST_ASSERT_EQUAL_PTR(&timer, control.timerFeed.root.next); // timer is attached, for real
    TEST_ASSERT_EQUAL_PTR(0, timer.next); // timer is attached only once
}

void test_manual_fire_from_interrupt_call_03(){
    // attached, aperiodic timer
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, false, timerFiredCallback);

    control.begin();
    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 20);
    control.attachTimer(&timer);

    TEST_ASSERT_FALSE(timerFiredFlag); // no premature fire
    control.manualFire(&timer);
    idleOnDummy(); // dummy write, to give some time to interrupt generation before testing results
    
    TEST_ASSERT_TRUE(timerFiredFlag); // timer fired as expected
    TEST_ASSERT_FALSE(timer.running); // timer is one shot, it was removed after firing
    TEST_ASSERT_EQUAL_PTR(0, control.timerFeed.root.next); // timer is not attached, for real
}

void test_manual_fire_from_interrupt_call_04(){
    // attached, periodic timer
    
    TimerArrayControl& control = *hcontrol;
    Timer timer(100, true, timerFiredCallback);

    control.begin();
    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 20);
    control.attachTimer(&timer);
    __HAL_TIM_SET_COUNTER(control.timerFeed.htim, 50); // time when manual fire happens

    TEST_ASSERT_FALSE(timerFiredFlag); // no premature fire
    control.manualFire(&timer);
    idleOnDummy(); // dummy write, to give some time to interrupt generation before testing results
    
    TEST_ASSERT_TRUE(timerFiredFlag); // timer fired as expected
    TEST_ASSERT_TRUE(timer.running); // timer is periodic, reattach is needed
    TEST_ASSERT_EQUAL(150, timer.target); // target was adjusted, according to time when fired
    TEST_ASSERT_EQUAL_PTR(&timer, control.timerFeed.root.next); // timer is attached, for real
    TEST_ASSERT_EQUAL_PTR(0, timer.next); // timer is attached only once
}

// TODO multiple timer scenarios
// timer order on attach
// timer order on attach in sync
// timer order on delay change


// -----                      -----
// ----- Test timing accuracy -----
// -----                      -----

uint32_t timeDifference(uint32_t t2, uint32_t t1){ // returns ticks from t1 to t2 in counter modulo
    return hcontrol->timerFeed.max_count & ((uint32_t)(t2 - t1));
}

struct stats_t {
    uint32_t firsttick = 0;
    uint32_t prevtick = 0;
    uint32_t int_tick = 0; // integral tick count, adjusted for one period
    uint32_t period; // the timer's period

    // limits of two sequential fires
    uint32_t mindelay = -1; // wraparound to maximum value
    uint32_t maxdelay = 0;

    // limits of integral errors
    uint32_t int_mindelay = -1; // wraparound to maximum value
    uint32_t int_maxdelay = 0;

    void reset(TIM_HandleTypeDef* htim, uint32_t _period){
        period = _period;
        mindelay = -1;
        maxdelay = 0;
        int_mindelay = -1;
        int_maxdelay = 0;
        int_tick = period;
        prevtick = __HAL_TIM_GET_COUNTER(htim);
        firsttick = prevtick;
    }

    void update(TIM_HandleTypeDef* htim){
        uint32_t tick = __HAL_TIM_GET_COUNTER(htim);
        uint32_t diff = timeDifference(tick, prevtick); // ticks since last callback
        uint32_t int_diff = int_tick + diff - period;

        mindelay = mindelay > diff ? diff : mindelay;
        maxdelay = maxdelay < diff ? diff : maxdelay;

        int_mindelay = int_mindelay > int_diff ? int_diff : int_mindelay;
        int_maxdelay = int_maxdelay < int_diff ? int_diff : int_maxdelay;

        prevtick = tick;
    }
} timingStats;

void callbackTimingStats(){
    timingStats.update(hcontrol->timerFeed.htim);
}

void callbackTimingStatsCtx(stats_t* stats){
    stats->update(hcontrol->timerFeed.htim);
}


void test_timing_accuracy_01(){
    // 100 Hz periodic firing
    const uint32_t delay = fcnt/100;

    // timing tolerances
    const uint32_t acceptable_earlyness_ticks = 0;
    const uint32_t acceptable_lateness_ticks = 0;
    const uint32_t acceptable_integral_earlyness_ticks = 0;
    const uint32_t acceptable_integral_lateness_ticks = 0;

    TimerArrayControl& control = *hcontrol;
    Timer timer(delay, true, callbackTimingStats);

    timingStats.reset(control.timerFeed.htim, delay); // reset stats
    control.attachTimer(&timer); // attach timer
    control.begin(); // start hardware timer

    HAL_Delay(1000); // accumulate results

    control.stop(); // stop interrupt generation
    
    UnityPrintNumber(delay - timingStats.mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.maxdelay - delay); UnityPrint(",");
    UnityPrintNumber(delay - timingStats.int_mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.int_maxdelay - delay); UnityPrint(" ");
    
    // evaluate results
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_earlyness_ticks, delay - timingStats.mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_lateness_ticks, timingStats.maxdelay - delay);

    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_earlyness_ticks, delay - timingStats.int_mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_lateness_ticks, timingStats.int_maxdelay - delay);
}

void test_timing_accuracy_02(){
    // 1 kHz periodic firing
    const uint32_t delay = fcnt/1000;

    // timing tolerances
    const uint32_t acceptable_earlyness_ticks = 0;
    const uint32_t acceptable_lateness_ticks = 0;
    const uint32_t acceptable_integral_earlyness_ticks = 0;
    const uint32_t acceptable_integral_lateness_ticks = 0;

    TimerArrayControl& control = *hcontrol;
    Timer timer(delay, true, callbackTimingStats);

    timingStats.reset(control.timerFeed.htim, delay); // reset stats
    control.attachTimer(&timer); // attach timer
    control.begin(); // start hardware timer

    HAL_Delay(1000); // accumulate results

    control.stop(); // stop interrupt generation
    
    UnityPrintNumber(delay - timingStats.mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.maxdelay - delay); UnityPrint(",");
    UnityPrintNumber(delay - timingStats.int_mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.int_maxdelay - delay); UnityPrint(" ");
    
    // evaluate results
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_earlyness_ticks, delay - timingStats.mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_lateness_ticks, timingStats.maxdelay - delay);

    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_earlyness_ticks, delay - timingStats.int_mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_lateness_ticks, timingStats.int_maxdelay - delay);
}

void test_timing_accuracy_03(){
    // 10 kHz periodic firing
    const uint32_t delay = fcnt/10000;

    // timing tolerances
    const uint32_t acceptable_earlyness_ticks = 0;
    const uint32_t acceptable_lateness_ticks = 0;
    const uint32_t acceptable_integral_earlyness_ticks = 0;
    const uint32_t acceptable_integral_lateness_ticks = 0;

    TimerArrayControl& control = *hcontrol;
    Timer timer(delay, true, callbackTimingStats);

    timingStats.reset(control.timerFeed.htim, delay); // reset stats
    control.attachTimer(&timer); // attach timer
    control.begin(); // start hardware timer

    HAL_Delay(1000); // accumulate results

    control.stop(); // stop interrupt generation
    
    UnityPrintNumber(delay - timingStats.mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.maxdelay - delay); UnityPrint(",");
    UnityPrintNumber(delay - timingStats.int_mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.int_maxdelay - delay); UnityPrint(" ");
    
    // evaluate results
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_earlyness_ticks, delay - timingStats.mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_lateness_ticks, timingStats.maxdelay - delay);

    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_earlyness_ticks, delay - timingStats.int_mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_lateness_ticks, timingStats.int_maxdelay - delay);
}

void test_timing_accuracy_04(){
    // 20 kHz periodic firing
    const uint32_t delay = fcnt/20000;

    // timing tolerances
    const uint32_t acceptable_earlyness_ticks = 0;
    const uint32_t acceptable_lateness_ticks = 0;
    const uint32_t acceptable_integral_earlyness_ticks = 0;
    const uint32_t acceptable_integral_lateness_ticks = 0;

    TimerArrayControl& control = *hcontrol;
    Timer timer(delay, true, callbackTimingStats);

    timingStats.reset(control.timerFeed.htim, delay); // reset stats
    control.attachTimer(&timer); // attach timer
    control.begin(); // start hardware timer

    HAL_Delay(1000); // accumulate results

    control.stop(); // stop interrupt generation
    
    UnityPrintNumber(delay - timingStats.mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.maxdelay - delay); UnityPrint(",");
    UnityPrintNumber(delay - timingStats.int_mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.int_maxdelay - delay); UnityPrint(" ");
    
    // evaluate results
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_earlyness_ticks, delay - timingStats.mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_lateness_ticks, timingStats.maxdelay - delay);

    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_earlyness_ticks, delay - timingStats.int_mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_lateness_ticks, timingStats.int_maxdelay - delay);
}

void test_timing_accuracy_05(){
    // 50 kHz periodic firing
    const uint32_t delay = fcnt/50000;

    // timing tolerances
    const uint32_t acceptable_earlyness_ticks = 0;
    const uint32_t acceptable_lateness_ticks = 0;
    const uint32_t acceptable_integral_earlyness_ticks = 0;
    const uint32_t acceptable_integral_lateness_ticks = 0;

    TimerArrayControl& control = *hcontrol;
    Timer timer(delay, true, callbackTimingStats);

    timingStats.reset(control.timerFeed.htim, delay); // reset stats
    control.attachTimer(&timer); // attach timer
    control.begin(); // start hardware timer

    HAL_Delay(1000); // accumulate results

    control.stop(); // stop interrupt generation
    
    UnityPrintNumber(delay - timingStats.mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.maxdelay - delay); UnityPrint(",");
    UnityPrintNumber(delay - timingStats.int_mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.int_maxdelay - delay); UnityPrint(" ");
    
    // evaluate results
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_earlyness_ticks, delay - timingStats.mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_lateness_ticks, timingStats.maxdelay - delay);

    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_earlyness_ticks, delay - timingStats.int_mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_lateness_ticks, timingStats.int_maxdelay - delay);
}

void test_timing_accuracy_06(){
    // 100 kHz periodic firing
    const uint32_t delay = fcnt/100000;

    // timing tolerances
    const uint32_t acceptable_earlyness_ticks = 0;
    const uint32_t acceptable_lateness_ticks = 0;
    const uint32_t acceptable_integral_earlyness_ticks = 0;
    const uint32_t acceptable_integral_lateness_ticks = 0;

    TimerArrayControl& control = *hcontrol;
    Timer timer(delay, true, callbackTimingStats);

    timingStats.reset(control.timerFeed.htim, delay); // reset stats
    control.attachTimer(&timer); // attach timer
    control.begin(); // start hardware timer

    HAL_Delay(1000); // accumulate results

    control.stop(); // stop interrupt generation
    
    UnityPrintNumber(delay - timingStats.mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.maxdelay - delay); UnityPrint(",");
    UnityPrintNumber(delay - timingStats.int_mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.int_maxdelay - delay); UnityPrint(" ");
    
    // evaluate results
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_earlyness_ticks, delay - timingStats.mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_lateness_ticks, timingStats.maxdelay - delay);

    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_earlyness_ticks, delay - timingStats.int_mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_lateness_ticks, timingStats.int_maxdelay - delay);
}

void test_timing_accuracy_07(){
    // 1 kHz periodic firing with context timer
    const uint32_t delay = fcnt/1000;

    // timing tolerances
    const uint32_t acceptable_earlyness_ticks = 0;
    const uint32_t acceptable_lateness_ticks = 0;
    const uint32_t acceptable_integral_earlyness_ticks = 0;
    const uint32_t acceptable_integral_lateness_ticks = 0;

    TimerArrayControl& control = *hcontrol;
    stats_t stats;
    stats.reset(control.timerFeed.htim, delay);
    ContextTimer<stats_t> timer(delay, true, &stats, callbackTimingStatsCtx);

    control.attachTimer(&timer); // attach timer
    control.begin(); // start hardware timer

    HAL_Delay(1000); // accumulate results

    control.stop(); // stop interrupt generation
    
    UnityPrintNumber(delay - stats.mindelay); UnityPrint(",");
    UnityPrintNumber(stats.maxdelay - delay); UnityPrint(",");
    UnityPrintNumber(delay - stats.int_mindelay); UnityPrint(",");
    UnityPrintNumber(stats.int_maxdelay - delay); UnityPrint(" ");
    
    // evaluate results
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_earlyness_ticks, delay - stats.mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_lateness_ticks, stats.maxdelay - delay);

    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_earlyness_ticks, delay - stats.int_mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_lateness_ticks, stats.int_maxdelay - delay);
}

void test_timing_accuracy_08(){
    // 100 Hz vs 1 kHz synchronized firing
    const uint32_t delay100 =  fcnt/100;
    const uint32_t delay1000 = fcnt/1000;

    // timing tolerances (on a >= 100kHz clock)
    TEST_ASSERT_GREATER_OR_EQUAL(100'000, fcnt);
    const uint32_t acceptable_earlyness_ticks = 0;
    const uint32_t acceptable_lateness_ticks = 1;
    const uint32_t acceptable_integral_earlyness_ticks = 0;
    const uint32_t acceptable_integral_lateness_ticks = 1;

    TimerArrayControl& control = *hcontrol;

    stats_t stats100;
    stats100.reset(control.timerFeed.htim, delay100);
    ContextTimer<stats_t> timer100(delay100, true, &stats100, callbackTimingStatsCtx);

    stats_t stats1000;
    stats1000.reset(control.timerFeed.htim, delay1000);
    ContextTimer<stats_t> timer1000(delay1000, true, &stats1000, callbackTimingStatsCtx);

    control.attachTimer(&timer100);
    control.attachTimer(&timer1000);
    control.begin(); // start hardware timer

    HAL_Delay(1000); // accumulate results

    control.stop(); // stop interrupt generation
    
    UnityPrintNumber(delay100 - stats100.mindelay); UnityPrint(",");
    UnityPrintNumber(stats100.maxdelay - delay100); UnityPrint(",");
    UnityPrintNumber(delay100 - stats100.int_mindelay); UnityPrint(",");
    UnityPrintNumber(stats100.int_maxdelay - delay100); UnityPrint("|");
    
    UnityPrintNumber(delay1000 - stats1000.mindelay); UnityPrint(",");
    UnityPrintNumber(stats1000.maxdelay - delay1000); UnityPrint(",");
    UnityPrintNumber(delay1000 - stats1000.int_mindelay); UnityPrint(",");
    UnityPrintNumber(stats1000.int_maxdelay - delay1000); UnityPrint(" ");
    
    // evaluate results
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_earlyness_ticks, delay100 - stats100.mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_lateness_ticks, stats100.maxdelay - delay100);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_earlyness_ticks, delay100 - stats100.int_mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_lateness_ticks, stats100.int_maxdelay - delay100);

    TEST_ASSERT_LESS_OR_EQUAL(acceptable_earlyness_ticks, delay1000 - stats1000.mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_lateness_ticks, stats1000.maxdelay - delay1000);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_earlyness_ticks, delay1000 - stats1000.int_mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_lateness_ticks, stats1000.int_maxdelay - delay1000);
}

void test_timing_accuracy_09(){
    // 100 Hz sleep
    const uint32_t delay =  fcnt/100;

    // timing tolerances (on a >= 100kHz clock)
    TEST_ASSERT_GREATER_OR_EQUAL(100'000, fcnt);
    const uint32_t acceptable_earlyness_ticks = 0;
    const uint32_t acceptable_lateness_ticks = 0;
    const uint32_t acceptable_integral_earlyness_ticks = 0;
    const uint32_t acceptable_integral_lateness_ticks = 0;

    TimerArrayControl& control = *hcontrol;

    control.begin(); // start hardware timer
    
    timingStats.reset(control.timerFeed.htim, delay);
    uint32_t time = HAL_GetTick();
    uint32_t cycles = 0;

    while(HAL_GetTick() - time < 1000){
        control.sleep(delay);
        timingStats.update(control.timerFeed.htim);
        ++cycles;
    }

    control.stop(); // stop interrupt generation
    
    UnityPrintNumber(delay - timingStats.mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.maxdelay - delay); UnityPrint(",");
    UnityPrintNumber(delay - timingStats.int_mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.int_maxdelay - delay); UnityPrint(" ");
    
    // evaluate results
    TEST_ASSERT_EQUAL(100, cycles);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_earlyness_ticks, delay - timingStats.mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_lateness_ticks, timingStats.maxdelay - delay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_earlyness_ticks, delay - timingStats.int_mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_lateness_ticks, timingStats.int_maxdelay - delay);
}

void test_timing_accuracy_10(){
    // 1 kHz sleep
    const uint32_t delay =  fcnt/1000;

    // timing tolerances (on a >= 100kHz clock)
    TEST_ASSERT_GREATER_OR_EQUAL(100'000, fcnt);
    const uint32_t acceptable_earlyness_ticks = 0;
    const uint32_t acceptable_lateness_ticks = 0;
    const uint32_t acceptable_integral_earlyness_ticks = 0;
    const uint32_t acceptable_integral_lateness_ticks = 0;

    TimerArrayControl& control = *hcontrol;

    control.begin(); // start hardware timer
    
    timingStats.reset(control.timerFeed.htim, delay);
    uint32_t time = HAL_GetTick();
    uint32_t cycles = 0;

    while(HAL_GetTick() - time < 1000){
        control.sleep(delay);
        timingStats.update(control.timerFeed.htim);
        ++cycles;
    }

    control.stop(); // stop interrupt generation
    
    UnityPrintNumber(delay - timingStats.mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.maxdelay - delay); UnityPrint(",");
    UnityPrintNumber(delay - timingStats.int_mindelay); UnityPrint(",");
    UnityPrintNumber(timingStats.int_maxdelay - delay); UnityPrint(" ");
    
    // evaluate results
    TEST_ASSERT_EQUAL(1000, cycles);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_earlyness_ticks, delay - timingStats.mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_lateness_ticks, timingStats.maxdelay - delay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_earlyness_ticks, delay - timingStats.int_mindelay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_integral_lateness_ticks, timingStats.int_maxdelay - delay);
}

void test_timing_accuracy_11(){
    // 1 Hz sleep (longer than max timer delay!)
    const uint32_t delay =  fcnt/1;

    TimerArrayControl& control = *hcontrol;

    // timing tolerances (on a >= 100kHz clock)
    TEST_ASSERT_GREATER_OR_EQUAL(100'000, fcnt);
    TEST_ASSERT_LESS_THAN(100'000, control.timerFeed.max_count);
    const uint32_t acceptable_hal_earlyness_ticks = 0;
    const uint32_t acceptable_hal_lateness_ticks = 0;

    control.begin(); // start hardware timer
    
    uint32_t hal_delay = HAL_GetTick();

    control.sleep(delay);
    hal_delay = HAL_GetTick() - hal_delay;    

    control.stop(); // stop interrupt generation
    
    UnityPrintNumber(1000 - hal_delay); UnityPrint(",");
    UnityPrintNumber(hal_delay - 1000); UnityPrint(" ");
    
    // evaluate results
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_hal_earlyness_ticks, 1000 - hal_delay);
    TEST_ASSERT_LESS_OR_EQUAL(acceptable_hal_lateness_ticks, hal_delay - 1000);
}


// -----                                    -----
// ----- Test a bunch of convoluted actions -----
// -----                                    -----

// TODO
// 3+ different convoluted scenarios

void test_convoluted_actions_01(){ // test rapid attach requests
    TimerArrayControl& control = *hcontrol;
    
    // Timer(delay, periodic, callback)
    Timer timers[15] = {
        {50000,false,0},{51000,false,0},{52000,false,0},{53000,false,0},{54000,false,0},
        {55000,false,0},{56000,false,0},{57000,false,0},{58000,false,0},{59000,false,0},
        {60000,false,0},{61000,false,0},{62000,false,0},{63000,false,0},{64000,false,0}
        };
    
    for (uint32_t i = 1; i < sizeof(timers)/sizeof(Timer); ++i) {
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(
            timers[i-1].delay(),
            timers[i].delay(),
            "targets in 'timers' array is not monotone increasing"
            );
    }

    control.begin();

    // not in a cycle to be more rapid
    control.attachTimer(&timers[0]);
    control.attachTimer(&timers[1]);
    control.attachTimer(&timers[2]);
    control.attachTimer(&timers[3]);
    control.attachTimer(&timers[4]);
    control.attachTimer(&timers[5]);
    control.attachTimer(&timers[6]);
    control.attachTimer(&timers[7]);
    control.attachTimer(&timers[8]);
    control.attachTimer(&timers[9]);
    control.attachTimer(&timers[10]);
    control.attachTimer(&timers[11]);
    control.attachTimer(&timers[12]);
    control.attachTimer(&timers[13]);
    control.attachTimer(&timers[14]);

    // we dont actually want timers to fire
    control.stop();

    char msg[100];
    for (uint32_t i = 1; i < sizeof(timers)/sizeof(Timer); ++i) {

        sprintf(msg, "timers[%lu].next is not timers[%lu]", i-1, i);
        TEST_ASSERT_EQUAL_PTR_MESSAGE(
            &timers[i],
            timers[i-1].next,
            msg
            );
        
        sprintf(msg, "timers[%lu] is not running", i);
        TEST_ASSERT_TRUE_MESSAGE(timers[i].isRunning(), msg);
    }
}


void test_convoluted_actions_02(){ // test firing on empty feed
    TimerArrayControl& control = *hcontrol;
    
    Timer timer(10, false, 0);

    control.attachTimer(&timer);
    control.begin();
    control.detachTimer(&timer);
    control.isTickOngoing = true; // won't affect tick but indicates if tick was executed
    uint32_t time = HAL_GetTick();
    while(control.isTickOngoing && HAL_GetTick() - time < 6600); // wait for actual fire

    TEST_ASSERT_FALSE(control.isTickOngoing);
}

// -----                -----
// ----- Test execution -----
// -----                -----

void setUp(){
    hwsetup_internal_timing();
    timerFiredFlag = false;
    hcontrol = new TimerArrayControl(&htim2, fclk, clkdiv, 16);
}

void tearDown(){
    hcontrol->stop();
    delete hcontrol;
    hcontrol = nullptr;
    hwteardown_internal_timing();
}

int main() {

    hwsetup_init();

    UNITY_BEGIN();
    
    // Test callback and interrupt mechanism
    RUN_TEST(test_interrupt_blocking_of_request);
    RUN_TEST(test_callback_chain_ctor_and_dtor);

    // Test API request registration
    RUN_TEST(test_attach_request_registration);
    RUN_TEST(test_detach_request_registration);
    RUN_TEST(test_delay_change_request_registration);
    RUN_TEST(test_attach_in_sync_request_registration);
    RUN_TEST(test_manual_fire_request_registration);

    // Test API request handling from direct calls
    RUN_TEST(test_attach_timer_from_direct_call_01);
    RUN_TEST(test_detach_timer_from_direct_call_01);
    RUN_TEST(test_delay_change_from_direct_call_01);
    RUN_TEST(test_attach_in_sync_from_direct_call_01);
    RUN_TEST(test_manual_fire_from_direct_call_01);
    RUN_TEST(test_manual_fire_from_direct_call_02);
    RUN_TEST(test_manual_fire_from_direct_call_03);
    RUN_TEST(test_manual_fire_from_direct_call_04);

    // Test API request handling from interrupt calls
    RUN_TEST(test_attach_timer_from_interrupt_call_01);
    RUN_TEST(test_detach_timer_from_interrupt_call_01);
    RUN_TEST(test_delay_change_from_interrupt_call_01);
    RUN_TEST(test_attach_in_sync_from_interrupt_call_01);
    RUN_TEST(test_manual_fire_from_interrupt_call_01);
    RUN_TEST(test_manual_fire_from_interrupt_call_02);
    RUN_TEST(test_manual_fire_from_interrupt_call_03);
    RUN_TEST(test_manual_fire_from_interrupt_call_04);

    // Test timing accuracy, run 3 times to have some confidence
    for (int i = 0; i < 3; ++i) {
        RUN_TEST(test_timing_accuracy_01);
        RUN_TEST(test_timing_accuracy_02);
        RUN_TEST(test_timing_accuracy_03);
        RUN_TEST(test_timing_accuracy_04);
        RUN_TEST(test_timing_accuracy_05);
        // RUN_TEST(test_timing_accuracy_06);
        RUN_TEST(test_timing_accuracy_07);
        RUN_TEST(test_timing_accuracy_08);
        RUN_TEST(test_timing_accuracy_09);
        RUN_TEST(test_timing_accuracy_10);
        RUN_TEST(test_timing_accuracy_11);
    }

    // Test a bunch of convoluted actions
    RUN_TEST(test_convoluted_actions_01);
    RUN_TEST(test_convoluted_actions_02);
    // RUN_TEST(test_convoluted_actions_03);

    UNITY_END();

    while(1){}
}
