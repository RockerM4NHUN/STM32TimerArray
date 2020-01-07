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

void test_timer_array_control_constructor(){
    // <ignored>, fclk, clkdiv and bits are set
    TimerArrayControl control((TIM_HandleTypeDef*)0x1234, 5000, 133, 20);

    TEST_ASSERT_EQUAL(5000, control.fclk);
    TEST_ASSERT_EQUAL(133, control.clkdiv);
    TEST_ASSERT_EQUAL(20, control.timerFeed.bits);
    TEST_ASSERT_EQUAL_PTR(0x1234, control.timerFeed.htim);
    TEST_ASSERT_EQUAL(TimerArrayControl::Request::NONE, control.request);
    TEST_ASSERT_FALSE(control.isTickOngoing);

    // other request variables can be ignored, they will be overwritten
    // when a request is made
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
    { TimerArrayControl control(nullptr, 5000, 1,  1); TEST_ASSERT_EQUAL(         1, control.timerFeed.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1,  4); TEST_ASSERT_EQUAL(        15, control.timerFeed.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1,  8); TEST_ASSERT_EQUAL(       255, control.timerFeed.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 11); TEST_ASSERT_EQUAL(      2047, control.timerFeed.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 13); TEST_ASSERT_EQUAL(      8191, control.timerFeed.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 14); TEST_ASSERT_EQUAL(     16383, control.timerFeed.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 15); TEST_ASSERT_EQUAL(     32767, control.timerFeed.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 16); TEST_ASSERT_EQUAL(     65535, control.timerFeed.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 20); TEST_ASSERT_EQUAL(   1048575, control.timerFeed.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 22); TEST_ASSERT_EQUAL(   4194303, control.timerFeed.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 27); TEST_ASSERT_EQUAL( 134217727, control.timerFeed.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 30); TEST_ASSERT_EQUAL(1073741823, control.timerFeed.max_count); }
    { TimerArrayControl control(nullptr, 5000, 1, 32); TEST_ASSERT_EQUAL(4294967295, control.timerFeed.max_count); }
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

#define CREATE_EMPTY_TIMER_FEED \
    TIM_TypeDef hw_tim;\
    uint32_t cnt = 33;\
    hw_tim.CNT = cnt;\
    TIM_HandleTypeDef htim;\
    htim.Instance = &hw_tim;\
    TimerArrayControl control(&htim);\
    (void)0 // force semicolon after define

#define CREATE_FILLED_TIMER_FEED \
    TIM_TypeDef hw_tim;\
    uint32_t cnt = 33;\
    hw_tim.CNT = cnt;\
    TIM_HandleTypeDef htim;\
    htim.Instance = &hw_tim;\
    TimerArrayControl control(&htim);\
    Timer timers[5] = {{100,false,0},{110,false,0},{120,false,0},{130,false,0},{140,false,0}};\
    control.timerFeed.root.next = &timers[0];\
    for (uint32_t i = 0; i < sizeof(timers)/sizeof(Timer); ++i) { timers[i].target = timers[i].delay; timers[i].running = true; }\
    control.timerFeed.htim->Instance->CCR1 = timers[0].target;\
    for (uint32_t i = 1; i < sizeof(timers)/sizeof(Timer); ++i) TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(timers[i-1].delay, timers[i].delay, "targets in 'timers' array is not monotone increasing");\
    for (uint32_t i = 1; i < sizeof(timers)/sizeof(Timer); ++i) timers[i-1].next = &timers[i];\
    (void)0 // force semicolon after define

void test_timer_feed_constructor(){
    CREATE_EMPTY_TIMER_FEED;

    TEST_ASSERT_EQUAL_PTR(&htim, control.timerFeed.htim); // timer is passed to timer feed
    TEST_ASSERT_EQUAL_PTR(nullptr, control.timerFeed.root.next); // timer feed is empty
}

void test_timer_feed_findTimerInsertionLink_01(){
    CREATE_EMPTY_TIMER_FEED;
    Timer timer(100, false, 0);
    timer.target = 100;

    // find link at first place with empty
    Timer* ins = control.timerFeed.findTimerInsertionLink(&control.timerFeed.root, &timer);
    TEST_ASSERT_EQUAL_PTR(&control.timerFeed.root, ins);
}

void test_timer_feed_findTimerInsertionLink_02(){
    CREATE_FILLED_TIMER_FEED;
    
    // find link at first place with non-empty
    Timer timer(99, false, 0);
    timer.target = 99;
    Timer* ins;

    ins = control.timerFeed.findTimerInsertionLink(&control.timerFeed.root, &timer);
    TEST_ASSERT_EQUAL_PTR(&control.timerFeed.root, ins);
    ins = control.timerFeed.findTimerInsertionLink(&timers[2], &timer);
    TEST_ASSERT_EQUAL_PTR(&timers[2], ins);
    ins = control.timerFeed.findTimerInsertionLink(&timers[4], &timer);
    TEST_ASSERT_EQUAL_PTR(&timers[4], ins);
}

void test_timer_feed_findTimerInsertionLink_03(){
    CREATE_FILLED_TIMER_FEED;

    // find link at mid place with non-empty
    Timer timer(131, false, 0);
    timer.target = 131;
    Timer* ins;

    ins = control.timerFeed.findTimerInsertionLink(&control.timerFeed.root, &timer);
    TEST_ASSERT_EQUAL_PTR(&timers[3], ins);
    ins = control.timerFeed.findTimerInsertionLink(&timers[0], &timer);
    TEST_ASSERT_EQUAL_PTR(&timers[3], ins);
    ins = control.timerFeed.findTimerInsertionLink(&timers[1], &timer);
    TEST_ASSERT_EQUAL_PTR(&timers[3], ins);
    ins = control.timerFeed.findTimerInsertionLink(&timers[2], &timer);
    TEST_ASSERT_EQUAL_PTR(&timers[3], ins);
    ins = control.timerFeed.findTimerInsertionLink(&timers[3], &timer);
    TEST_ASSERT_EQUAL_PTR(&timers[3], ins);
    ins = control.timerFeed.findTimerInsertionLink(&timers[4], &timer);
    TEST_ASSERT_EQUAL_PTR(&timers[4], ins);
}

void test_timer_feed_insertTimer_01(){
    CREATE_EMPTY_TIMER_FEED;
    Timer timer(123, false, 0);
    timer.target = 123;
    
    // insert into empty, at given place
    control.timerFeed.insertTimer(&control.timerFeed.root, &timer);
    TEST_ASSERT_EQUAL_PTR(&timer, control.timerFeed.root.next); // timer is in the feed at the first place
    TEST_ASSERT_EQUAL_PTR(nullptr, timer.next); // timer is the last in the feed
    TEST_ASSERT_TRUE(timer.running);
    TEST_ASSERT_EQUAL(123, hw_tim.CCR1); // test if hardware timer's capture/compare register is updated
}

void test_timer_feed_insertTimer_02(){
    CREATE_FILLED_TIMER_FEED;
    Timer timer(99, false, 0);
    timer.target = 99;
    
    // insert into filled at first place
    control.timerFeed.insertTimer(&timer);
    TEST_ASSERT_EQUAL_PTR(&timer, control.timerFeed.root.next); // timer is in the feed at the first place
    TEST_ASSERT_EQUAL_PTR(&timers[0], timer.next); // timer is before timers[0] in the feed
    TEST_ASSERT_TRUE(timer.running);
    TEST_ASSERT_EQUAL(99, hw_tim.CCR1); // test if hardware timer's capture/compare register is updated
}

void test_timer_feed_insertTimer_03(){
    CREATE_FILLED_TIMER_FEED;
    Timer timer(121, false, 0);
    timer.target = 121;
    
    // insert into filled at middle place
    control.timerFeed.insertTimer(&timer);
    TEST_ASSERT_EQUAL_PTR(&timer, timers[2].next); // timer is in the feed after timer with target of 120
    TEST_ASSERT_EQUAL_PTR(&timers[3], timer.next); // timer is before timers[3] in the feed
    TEST_ASSERT_TRUE(timer.running);
    TEST_ASSERT_EQUAL(100, hw_tim.CCR1); // test if hardware timer's capture/compare register is updated (it should not update to 111, but stay at 100)
}

void test_timer_feed_insertTimer_04(){
    CREATE_FILLED_TIMER_FEED;
    Timer timer(141, false, 0);
    timer.target = 141;
    
    // insert into filled at last place
    control.timerFeed.insertTimer(&timer);
    TEST_ASSERT_EQUAL_PTR(&timer, timers[4].next); // timer is in the feed after timer with target of 140 (the last one)
    TEST_ASSERT_EQUAL_PTR(nullptr, timer.next); // timer is before end of the feed
    TEST_ASSERT_TRUE(timer.running);
    TEST_ASSERT_EQUAL(100, hw_tim.CCR1); // test if hardware timer's capture/compare register is updated (it should not update to 141, but stay at 100)
}

void test_timer_feed_removeTimer_01(){
    CREATE_EMPTY_TIMER_FEED;
    Timer timer(123, false, 0);
    timer.target = 123;

    // insert timer before remove
    control.timerFeed.insertTimer(&timer);

    // remove from (almost) empty
    control.timerFeed.removeTimer(&timer);
    TEST_ASSERT_EQUAL_PTR(nullptr, control.timerFeed.root.next); // timer is not in the feed anymore
    TEST_ASSERT_EQUAL_PTR(nullptr, timer.next); // timer is not linked
    TEST_ASSERT_FALSE(timer.running);
}

void test_timer_feed_removeTimer_02(){
    CREATE_FILLED_TIMER_FEED;

    // remove from first position
    control.timerFeed.removeTimer(&timers[0]);
    TEST_ASSERT_EQUAL_PTR(&timers[1], control.timerFeed.root.next); // the next timer comes to front
    TEST_ASSERT_EQUAL_PTR(nullptr, timers[0].next); // timer is not linked
    TEST_ASSERT_FALSE(timers[0].running);
    TEST_ASSERT_EQUAL(110, hw_tim.CCR1); // test if hardware timer's capture/compare register is updated
}

void test_timer_feed_removeTimer_03(){
    CREATE_FILLED_TIMER_FEED;

    // remove from middle position
    control.timerFeed.removeTimer(&timers[1]);
    TEST_ASSERT_EQUAL_PTR(&timers[0], control.timerFeed.root.next); // the front did not change
    TEST_ASSERT_EQUAL_PTR(&timers[2], timers[0].next); // predecessor points to child
    TEST_ASSERT_EQUAL_PTR(nullptr, timers[1].next); // timer is not linked
    TEST_ASSERT_FALSE(timers[1].running);
    TEST_ASSERT_EQUAL(100, hw_tim.CCR1); // test if hardware timer's capture/compare register is updated (should not update from 100)
}

void test_timer_feed_removeTimer_04(){
    CREATE_FILLED_TIMER_FEED;

    // remove from last position
    control.timerFeed.removeTimer(&timers[4]);
    TEST_ASSERT_EQUAL_PTR(&timers[0], control.timerFeed.root.next); // the front did not change
    TEST_ASSERT_EQUAL_PTR(nullptr, timers[3].next); // predecessor points to the end
    TEST_ASSERT_EQUAL_PTR(nullptr, timers[4].next); // timer is not linked
    TEST_ASSERT_FALSE(timers[4].running);
    TEST_ASSERT_EQUAL(100, hw_tim.CCR1); // test if hardware timer's capture/compare register is updated (should not update from 100)
}

void test_timer_feed_updateTarget_01(){
    CREATE_EMPTY_TIMER_FEED;
    Timer timer(123, false, 0);
    timer.target = 123;

    // insert timer before update
    control.timerFeed.insertTimer(&timer);

    // delay in (almost) empty feed
    control.timerFeed.updateTarget(&timer, 234, cnt);
    TEST_ASSERT_EQUAL_PTR(&timer, control.timerFeed.root.next); // timer is still in the feed
    TEST_ASSERT_EQUAL_PTR(nullptr, timer.next); // timer is not linked
    TEST_ASSERT_TRUE(timer.running);
    TEST_ASSERT_EQUAL(234, timer.target);
    TEST_ASSERT_EQUAL(234, hw_tim.CCR1); // test if capture/compare register was updated too
}

void test_timer_feed_updateTarget_02(){
    CREATE_EMPTY_TIMER_FEED;
    Timer timer(123, false, 0);
    timer.target = 123;

    // insert timer before update
    control.timerFeed.insertTimer(&timer);

    // early in (almost) empty feed
    control.timerFeed.updateTarget(&timer, 12, cnt);
    TEST_ASSERT_EQUAL_PTR(&timer, control.timerFeed.root.next); // timer is still in the feed
    TEST_ASSERT_EQUAL_PTR(nullptr, timer.next); // timer is not linked
    TEST_ASSERT_TRUE(timer.running);
    TEST_ASSERT_EQUAL(12, timer.target);
    TEST_ASSERT_EQUAL(12, hw_tim.CCR1); // test if capture/compare register was updated too
}

void test_timer_feed_updateTarget_03(){
    CREATE_FILLED_TIMER_FEED;

    // update first place, no change target
    control.timerFeed.updateTarget(&timers[0], 100, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[0], control.timerFeed.root.next); // timer is still first
    TEST_ASSERT_EQUAL_PTR(&timers[1], timers[0].next); // timer is linked as was before
    TEST_ASSERT_TRUE(timers[0].running);
    TEST_ASSERT_EQUAL(100, timers[0].target); // target remained the same
    TEST_ASSERT_EQUAL(100, hw_tim.CCR1); // test if capture/compare register remained the same
}

void test_timer_feed_updateTarget_04(){
    CREATE_FILLED_TIMER_FEED;

    // update first place, early target
    control.timerFeed.updateTarget(&timers[0], 99, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[0], control.timerFeed.root.next); // timer is still first
    TEST_ASSERT_EQUAL_PTR(&timers[1], timers[0].next); // timer is linked as was before
    TEST_ASSERT_TRUE(timers[0].running);
    TEST_ASSERT_EQUAL(99, timers[0].target); // target was updated
    TEST_ASSERT_EQUAL(99, hw_tim.CCR1); // test if capture/compare register was updated
}

void test_timer_feed_updateTarget_05(){
    CREATE_FILLED_TIMER_FEED;

    // update first place, slightly delayed target
    control.timerFeed.updateTarget(&timers[0], 101, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[0], control.timerFeed.root.next); // timer is still first
    TEST_ASSERT_EQUAL_PTR(&timers[1], timers[0].next); // timer is linked as was before
    TEST_ASSERT_TRUE(timers[0].running);
    TEST_ASSERT_EQUAL(101, timers[0].target); // target was updated
    TEST_ASSERT_EQUAL(101, hw_tim.CCR1); // test if capture/compare register was updated
}

void test_timer_feed_updateTarget_06(){
    CREATE_FILLED_TIMER_FEED;

    // update first place, delayed to mid
    control.timerFeed.updateTarget(&timers[0], 121, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[1], control.timerFeed.root.next); // timer is not first anymore
    TEST_ASSERT_EQUAL_PTR(&timers[3], timers[0].next); // timer is linked as mid
    TEST_ASSERT_EQUAL_PTR(&timers[0], timers[2].next); // predecessor is linked as mid
    TEST_ASSERT_TRUE(timers[0].running);
    TEST_ASSERT_EQUAL(121, timers[0].target); // target was updated
    TEST_ASSERT_EQUAL(110, hw_tim.CCR1); // test if capture/compare register was updated
}

void test_timer_feed_updateTarget_07(){
    CREATE_FILLED_TIMER_FEED;

    // update first place, delayed to last
    control.timerFeed.updateTarget(&timers[0], 141, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[1], control.timerFeed.root.next); // timer is not first anymore
    TEST_ASSERT_EQUAL_PTR(nullptr, timers[0].next); // timer is linked as last
    TEST_ASSERT_EQUAL_PTR(&timers[0], timers[4].next); // predecessor is linked as mid
    TEST_ASSERT_TRUE(timers[0].running);
    TEST_ASSERT_EQUAL(141, timers[0].target); // target was updated
    TEST_ASSERT_EQUAL(110, hw_tim.CCR1); // test if capture/compare register was updated
}

void test_timer_feed_updateTarget_08(){
    CREATE_FILLED_TIMER_FEED;

    // update mid place, no change target
    control.timerFeed.updateTarget(&timers[2], 120, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[2], timers[1].next); // timer is still at it's place
    TEST_ASSERT_EQUAL_PTR(&timers[3], timers[2].next); // timer is linked as was before
    TEST_ASSERT_TRUE(timers[2].running);
    TEST_ASSERT_EQUAL(120, timers[2].target); // target remained the same
    TEST_ASSERT_EQUAL(100, hw_tim.CCR1); // test if capture/compare register remained the same
}

void test_timer_feed_updateTarget_09(){
    CREATE_FILLED_TIMER_FEED;

    // update mid place, early to first
    control.timerFeed.updateTarget(&timers[2], 99, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[2], control.timerFeed.root.next); // timer became first
    TEST_ASSERT_EQUAL_PTR(&timers[0], timers[2].next); // timer is linked as first
    TEST_ASSERT_EQUAL_PTR(&timers[3], timers[1].next); // predecessor is linked as mid
    TEST_ASSERT_TRUE(timers[2].running);
    TEST_ASSERT_EQUAL(99, timers[2].target); // target was updated
    TEST_ASSERT_EQUAL(99, hw_tim.CCR1); // test if capture/compare register was updated
}

void test_timer_feed_updateTarget_10(){
    CREATE_FILLED_TIMER_FEED;

    // update mid place, early to mid
    control.timerFeed.updateTarget(&timers[3], 119, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[3], timers[1].next); // timer became mid
    TEST_ASSERT_EQUAL_PTR(&timers[2], timers[3].next); // timer is linked as mid
    TEST_ASSERT_EQUAL_PTR(&timers[4], timers[2].next); // predecessor is linked as mid
    TEST_ASSERT_TRUE(timers[3].running);
    TEST_ASSERT_EQUAL(119, timers[3].target); // target was updated
    TEST_ASSERT_EQUAL(100, hw_tim.CCR1); // test if capture/compare register remained the same
}

void test_timer_feed_updateTarget_11(){
    CREATE_FILLED_TIMER_FEED;

    // update mid place, slightly early target
    control.timerFeed.updateTarget(&timers[2], 119, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[2], timers[1].next); // timer is still at it's place
    TEST_ASSERT_EQUAL_PTR(&timers[3], timers[2].next); // timer is linked as was before
    TEST_ASSERT_TRUE(timers[2].running);
    TEST_ASSERT_EQUAL(119, timers[2].target); // target was updated
    TEST_ASSERT_EQUAL(100, hw_tim.CCR1); // test if capture/compare register remained the same
}

void test_timer_feed_updateTarget_12(){
    CREATE_FILLED_TIMER_FEED;

    // update mid place, slightly delayed target
    control.timerFeed.updateTarget(&timers[2], 121, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[2], timers[1].next); // timer is still at it's place
    TEST_ASSERT_EQUAL_PTR(&timers[3], timers[2].next); // timer is linked as was before
    TEST_ASSERT_TRUE(timers[2].running);
    TEST_ASSERT_EQUAL(121, timers[2].target); // target was updated
    TEST_ASSERT_EQUAL(100, hw_tim.CCR1); // test if capture/compare register remained the same
}

void test_timer_feed_updateTarget_13(){
    CREATE_FILLED_TIMER_FEED;

    // update mid place, delayed to mid
    control.timerFeed.updateTarget(&timers[1], 121, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[1], timers[2].next); // timer became mid
    TEST_ASSERT_EQUAL_PTR(&timers[3], timers[1].next); // timer is linked as mid
    TEST_ASSERT_EQUAL_PTR(&timers[2], timers[0].next); // predecessor is linked as mid
    TEST_ASSERT_TRUE(timers[1].running);
    TEST_ASSERT_EQUAL(121, timers[1].target); // target was updated
    TEST_ASSERT_EQUAL(100, hw_tim.CCR1); // test if capture/compare register remained the same
}

void test_timer_feed_updateTarget_14(){
    CREATE_FILLED_TIMER_FEED;

    // update mid place, delayed to last
    control.timerFeed.updateTarget(&timers[1], 141, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[1], timers[4].next); // timer became last
    TEST_ASSERT_EQUAL_PTR(nullptr, timers[1].next); // timer is linked as last
    TEST_ASSERT_EQUAL_PTR(&timers[2], timers[0].next); // predecessor is linked as mid
    TEST_ASSERT_TRUE(timers[1].running);
    TEST_ASSERT_EQUAL(141, timers[1].target); // target was updated
    TEST_ASSERT_EQUAL(100, hw_tim.CCR1); // test if capture/compare register remained the same
}

void test_timer_feed_updateTarget_15(){
    CREATE_FILLED_TIMER_FEED;

    // update last place, no change target
    control.timerFeed.updateTarget(&timers[4], 140, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[4], timers[3].next); // timer is still at it's place
    TEST_ASSERT_EQUAL_PTR(nullptr, timers[4].next); // timer is linked as was before
    TEST_ASSERT_TRUE(timers[4].running);
    TEST_ASSERT_EQUAL(140, timers[4].target); // target remained the same
    TEST_ASSERT_EQUAL(100, hw_tim.CCR1); // test if capture/compare register remained the same
}

void test_timer_feed_updateTarget_16(){
    CREATE_FILLED_TIMER_FEED;

    // update last place, slightly early target
    control.timerFeed.updateTarget(&timers[4], 139, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[4], timers[3].next); // timer is still at it's place
    TEST_ASSERT_EQUAL_PTR(nullptr, timers[4].next); // timer is linked as was before
    TEST_ASSERT_TRUE(timers[4].running);
    TEST_ASSERT_EQUAL(139, timers[4].target); // target was updated
    TEST_ASSERT_EQUAL(100, hw_tim.CCR1); // test if capture/compare register remained the same
}

void test_timer_feed_updateTarget_17(){
    CREATE_FILLED_TIMER_FEED;

    // update last place, delayed target
    control.timerFeed.updateTarget(&timers[4], 141, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[4], timers[3].next); // timer is still at it's place
    TEST_ASSERT_EQUAL_PTR(nullptr, timers[4].next); // timer is linked as was before
    TEST_ASSERT_TRUE(timers[4].running);
    TEST_ASSERT_EQUAL(141, timers[4].target); // target was updated
    TEST_ASSERT_EQUAL(100, hw_tim.CCR1); // test if capture/compare register remained the same
}

void test_timer_feed_updateTarget_18(){
    CREATE_FILLED_TIMER_FEED;

    // update last place, early to mid
    control.timerFeed.updateTarget(&timers[4], 129, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[4], timers[2].next); // timer became mid
    TEST_ASSERT_EQUAL_PTR(&timers[3], timers[4].next); // timer is linked as mid
    TEST_ASSERT_EQUAL_PTR(nullptr, timers[3].next); // predecessor is linked as last
    TEST_ASSERT_TRUE(timers[4].running);
    TEST_ASSERT_EQUAL(129, timers[4].target); // target was updated
    TEST_ASSERT_EQUAL(100, hw_tim.CCR1); // test if capture/compare register remained the same
}

void test_timer_feed_updateTarget_19(){
    CREATE_FILLED_TIMER_FEED;

    // update last place, early to first
    control.timerFeed.updateTarget(&timers[4], 99, cnt);
    TEST_ASSERT_EQUAL_PTR(&timers[4], control.timerFeed.root.next); // timer became first
    TEST_ASSERT_EQUAL_PTR(&timers[0], timers[4].next); // timer is linked as first
    TEST_ASSERT_EQUAL_PTR(nullptr, timers[3].next); // predecessor is linked as last
    TEST_ASSERT_TRUE(timers[4].running);
    TEST_ASSERT_EQUAL(99, timers[4].target); // target was updated
    TEST_ASSERT_EQUAL(99, hw_tim.CCR1); // test if capture/compare register was updated
}

void test_timer_feed_updateTarget_20(){
    // test wraparound in this function, or create another function and use it in this one
    // and than test 1-2 wraparound cases here too (wraparound in both directions)
    TEST_FAIL_MESSAGE("not implemented");
}

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
    RUN_TEST(test_timer_array_control_constructor);
    RUN_TEST(test_prescale_settings);
    RUN_TEST(test_max_counter_value);
    RUN_TEST(test_fcnt_calculation);

    // Test TimerFeed functions
    RUN_TEST(test_timer_feed_constructor);
    RUN_TEST(test_timer_feed_findTimerInsertionLink_01);
    RUN_TEST(test_timer_feed_findTimerInsertionLink_02);
    RUN_TEST(test_timer_feed_findTimerInsertionLink_03);
    RUN_TEST(test_timer_feed_insertTimer_01);
    RUN_TEST(test_timer_feed_insertTimer_02);
    RUN_TEST(test_timer_feed_insertTimer_03);
    RUN_TEST(test_timer_feed_insertTimer_04);

    RUN_TEST(test_timer_feed_removeTimer_01);
    RUN_TEST(test_timer_feed_removeTimer_02);
    RUN_TEST(test_timer_feed_removeTimer_03);
    RUN_TEST(test_timer_feed_removeTimer_04);

    RUN_TEST(test_timer_feed_updateTarget_01);
    RUN_TEST(test_timer_feed_updateTarget_02);
    RUN_TEST(test_timer_feed_updateTarget_03);
    RUN_TEST(test_timer_feed_updateTarget_04);
    RUN_TEST(test_timer_feed_updateTarget_05);
    RUN_TEST(test_timer_feed_updateTarget_06);
    RUN_TEST(test_timer_feed_updateTarget_07);
    RUN_TEST(test_timer_feed_updateTarget_08);
    RUN_TEST(test_timer_feed_updateTarget_09);
    RUN_TEST(test_timer_feed_updateTarget_10);
    RUN_TEST(test_timer_feed_updateTarget_11);
    RUN_TEST(test_timer_feed_updateTarget_12);
    RUN_TEST(test_timer_feed_updateTarget_13);
    RUN_TEST(test_timer_feed_updateTarget_14);
    RUN_TEST(test_timer_feed_updateTarget_15);
    RUN_TEST(test_timer_feed_updateTarget_16);
    RUN_TEST(test_timer_feed_updateTarget_17);
    RUN_TEST(test_timer_feed_updateTarget_18);
    RUN_TEST(test_timer_feed_updateTarget_19);

    UNITY_END();

    while(1){}
}
