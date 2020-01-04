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
static uint8_t timerFired = 0;
void timerFiredCallback(){ timerFired = 1; }

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
    Timer timer(100, false, timerFiredCallback);

    control.begin(); // enables timer interrupt
    __HAL_TIM_DISABLE_IT(&htim2, TIM_IT_FLAG); // disable timer interrupt, to prevent tick
    
    // test for misconfigured controller, if fails the test setup is wrong
    TEST_ASSERT_EQUAL_PTR(0, control.timerFeed.root.next);

    // test for actual interrupt blocking, if fails the handler was called
    control.attachTimer(&timer);
    TEST_ASSERT_EQUAL_PTR(0, control.timerFeed.root.next);
    
    // test for reintroduction of the cached interrupt,
    // if fails the problem can also be in the attachTimer functionality
    __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_FLAG); // enable timer interrupt, to serve the attach request
    HAL_Delay(0); // add some cycles for interrupt to kick in
    TEST_ASSERT_EQUAL_PTR(&timer, control.timerFeed.root.next);
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

// TODO


// -----                                                -----
// ----- Test API request handling from interrupt calls -----
// -----                                                -----

// TODO



// -----                      -----
// ----- Test timing accuracy -----
// -----                      -----

void test_single_shot_delay_accuracy(){
    TEST_FAIL_MESSAGE("not implemented");
}


// -----                -----
// ----- Test execution -----
// -----                -----

void setUp(){
    hcontrol = new TimerArrayControl(&htim2, fclk, clkdiv, 16);
    hwsetup_internal_timing();
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
    
    RUN_TEST(test_interrupt_blocking_of_request);
    RUN_TEST(test_callback_chain_ctor_and_dtor);
    RUN_TEST(test_attach_request_registration);
    RUN_TEST(test_detach_request_registration);
    RUN_TEST(test_delay_change_request_registration);
    RUN_TEST(test_attach_in_sync_request_registration);
    RUN_TEST(test_manual_fire_request_registration);
    //RUN_TEST(test_single_shot_delay_accuracy);

    UNITY_END();

    while(1){}
}
