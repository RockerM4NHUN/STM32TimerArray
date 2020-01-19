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
static const uint32_t fcnt = 1'000'000; // above 100 kHz test numbers can be off by a few
static const uint16_t clkdiv = fclk / fcnt;
static TimerArrayControl* hcontrol = nullptr;
static bool timerFiredFlag;
void timerFiredCallback(){ timerFiredFlag = true; }
volatile uint8_t rwDummy;
uint8_t idleOnDummy(){ return rwDummy; }

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

void test_3_timers_in_perfect_sync_1sec(){
    // 100 kHz period
    const uint32_t delay =  fcnt/100'000;

    // timing tolerances (on a >= 200kHz clock)
    TEST_ASSERT_GREATER_OR_EQUAL(200'000, fcnt);
    const uint32_t acceptable_earlyness_ticks = 0;
    const uint32_t acceptable_lateness_ticks = 0;
    const uint32_t acceptable_integral_earlyness_ticks = 0;
    const uint32_t acceptable_integral_lateness_ticks = 0;

    TimerArrayControl& control = *hcontrol;
    Timer timer1(delay, true, callbackTimingStats);
    Timer timer2(delay, true, callbackTimingStats);
    Timer timer3(delay, true, callbackTimingStats);

    timingStats.reset(control.timerFeed.htim, delay); // reset stats
    control.attachTimer(&timer1); // attach timer
    control.attachTimer(&timer2); // attach timer
    control.attachTimer(&timer2); // attach timer
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
    
    uint32_t time_sum = 0;
    Timer* timers[]={&timer1, &timer2, &timer3};
    for (uint32_t i = 1; i < sizeof(timers)/sizeof(Timer*); ++i){
        time_sum += timeDifference(timers[i]->target, timers[i-1]->target);
    }
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(
        time_sum,
        65535,
        "targets in 'timers' array is not monotone increasing"
    );
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
    
    // 1 second tests
    RUN_TEST(test_3_timers_in_perfect_sync_1sec);
    //RUN_TEST(test_3_timers_almost_in_sync_1sec);

    // 1 minute tests
    //RUN_TEST(test_3_timers_in_perfect_sync_1min);
    //RUN_TEST(test_3_timers_almost_in_sync_1min);

    UNITY_END();

    while(1){}
}
