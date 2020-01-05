#include "../hwsetup.h"
#include <unity.h>

#include "TimerArrayControl.hpp"

/*
 * Test basic functionalities of TimerArrayControl, without using any peripherals.
 */

// -----                           -----
// ----- Always passing dummy test -----
// -----                           -----

void test_evaluation_of_an_empty_test(){
    // empty test should pass
}

// -----                                           -----
// ----- Test TimerArrayControl setup calculations -----
// -----                                           -----

void test_parameter_settings(){
    // <ignored>, fclk, clkdiv and bits are set
    TimerArrayControl control(nullptr, 5000, 133, 20);

    TEST_ASSERT_EQUAL(5000, control.fclk);
    TEST_ASSERT_EQUAL(133, control.clkdiv);
    TEST_ASSERT_EQUAL(20, control.bits);
}

void test_prescale_settings(){
    // prescaler bits are set to 16 and the max_value of the prescaler is 2^16 - 1
    {
        TimerArrayControl control(nullptr, 5000, 70000, 20);
        TEST_ASSERT_EQUAL(16, control.prescaler_bits);
        TEST_ASSERT_EQUAL(65536, control.max_prescale);
    }

    // prescaler value is limited according to prescaler bits
    {
        TimerArrayControl control(nullptr, 5000, 65534, 20);
        TEST_ASSERT_EQUAL(65534, control.prescaler);
    }

    {
        TimerArrayControl control(nullptr, 5000, 65537, 20);
        TEST_ASSERT_EQUAL(65536, control.prescaler);
    }
}

void test_max_counter_value(){
    // max value of the timer's N bit counter register is set to 2^N - 1
    { TimerArrayControl control(nullptr, 5000, 1,  1); TEST_ASSERT_EQUAL(         1, control.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1,  4); TEST_ASSERT_EQUAL(        15, control.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1,  8); TEST_ASSERT_EQUAL(       255, control.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 11); TEST_ASSERT_EQUAL(      2047, control.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 13); TEST_ASSERT_EQUAL(      8191, control.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 14); TEST_ASSERT_EQUAL(     16383, control.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 15); TEST_ASSERT_EQUAL(     32767, control.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 16); TEST_ASSERT_EQUAL(     65535, control.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 20); TEST_ASSERT_EQUAL(   1048575, control.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 22); TEST_ASSERT_EQUAL(   4194303, control.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 27); TEST_ASSERT_EQUAL( 134217727, control.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 30); TEST_ASSERT_EQUAL(1073741823, control.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 32); TEST_ASSERT_EQUAL(4294967295, control.max_count); }
}


void test_fcnt_calculation(){
    // actual counting frequency is the input frequency/prescaler
    { TimerArrayControl control(nullptr, 5000,     1, 16); TEST_ASSERT_FLOAT_WITHIN(   0, 5000.0,      control.actualTickFrequency()); }
    { TimerArrayControl control(nullptr, 5000,   133, 16); TEST_ASSERT_FLOAT_WITHIN(1e-3,   37.593984, control.actualTickFrequency()); }
    { TimerArrayControl control(nullptr, 2000, 10000, 16); TEST_ASSERT_FLOAT_WITHIN(1e-5,    0.2,      control.actualTickFrequency()); }
    { TimerArrayControl control(nullptr, 5000, 13621, 16); TEST_ASSERT_FLOAT_WITHIN(1e-5,    0.367080, control.actualTickFrequency()); }

    { TimerArrayControl control(nullptr, 250'000'000, 1, 16); TEST_ASSERT_FLOAT_WITHIN(250e1, 250e6, control.actualTickFrequency()); }
    { TimerArrayControl control(nullptr, 250'000'000, 3, 16); TEST_ASSERT_FLOAT_WITHIN(250e1, 83333333.3333, control.actualTickFrequency()); }
    { TimerArrayControl control(nullptr, 250'000'000, 65536, 16); TEST_ASSERT_FLOAT_WITHIN(250e1, 3814.69726, control.actualTickFrequency()); }
}

// -----                          -----
// ----- Test TimerFeed functions -----
// -----                          -----

// TODO


// -----                -----
// ----- Test execution -----
// -----                -----

void setUp(){
    // no hardware component is used
}

void tearDown(){
    // no hardware component is used
}

int main() {

    hwsetup_init();

    UNITY_BEGIN();
    
    // Always passing dummy test
    RUN_TEST(test_evaluation_of_an_empty_test);
    
    // Test TimerArrayControl setup calculations
    RUN_TEST(test_parameter_settings);
    RUN_TEST(test_prescale_settings);
    RUN_TEST(test_max_counter_value);
    RUN_TEST(test_fcnt_calculation);

    // Test TimerFeed functions

    UNITY_END();

    while(1){}
}
