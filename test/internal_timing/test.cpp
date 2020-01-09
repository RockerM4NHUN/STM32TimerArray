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
 * SysTick and the library implementation.
 */

// Test helpers, might be transferred to hwsetup
static const auto TIM_IT_FLAG = TIM_IT_CC1;
static const uint32_t fclk = 72'000'000;
static const uint32_t fcnt = 10'000;
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
    void f(TIM_HandleTypeDef* htim){ *handle = htim; *hflag = 1; }
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
    TEST_ASSERT_EQUAL(123, timer.delay); // the delay was updated
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
    
    TEST_ASSERT_EQUAL(123, timer.delay); // the delay was updated
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

void test_single_shot_delay_accuracy_01(){
    TEST_FAIL_MESSAGE("not implemented");
}
// TODO: multiple timers firing at once

// -----                                    -----
// ----- Test a bunch of convoluted actions -----
// -----                                    -----

// TODO
// 3 different convoluted scenarios

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

    // Test timing accuracy
    // RUN_TEST(test_single_shot_delay_accuracy_01);

    // Test a bunch of convoluted actions
    // RUN_TEST(test_convoluted_actions_01);
    // RUN_TEST(test_convoluted_actions_02);
    // RUN_TEST(test_convoluted_actions_03);

    UNITY_END();

    while(1){}
}
