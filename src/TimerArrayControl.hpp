#pragma once

extern "C"{
    #include "tim.h"
}

#include "CallbackChain.hpp"


// TimerArrayControl callback chain setup
struct TAC_CallbackChainID{};
using TAC_CallbackChain = CallbackChain<TAC_CallbackChainID, TIM_HandleTypeDef*>;


// Represents a timer, handled by a TimerArrayControl object.
// Attach it to a controller to receive callbacks.
//
// period: ticks of timer array controller until firing
// isPeriodic: does the timer restart immedietely after firing
// f: function called when timer is firing
class Timer{
public:
    using callback_function = void(*)();
    Timer(const callback_function f);
    Timer(uint32_t period, bool isPeriodic, const callback_function f);
    
    bool isRunning();

    // Changing the timers period will not affect the current firing event, only the next one.
    // To restart the timer with the new period detach and attach it.
    // To change the timer's period without restart, use the changeTimerPeriod method
    // in the TimerArrayControl.
    uint32_t period; // required period of timer (in ticks)
    bool isPeriodic; // should the timer be immedietely restarted after firing

protected:
    uint32_t target; // counter value that the timer fires at next
    const callback_function f;
    bool running;
    Timer* next;

    friend class TimerArrayControl;
};



// Implements timer controller for hardware handling,
// it encapsulates any hardware related issue and presents a simple common API.
// Requires a timer with capture compare capabilities (implementing a timer array
// with only a resetting counter requires significantly more computations).
// Sets 1, 2 or 4 additional clock division if necessary.
// By default has 100 kHz tick speed, a 1 ms delay needs a tick value of 100.
// 
// fclk: timer's input clock speed, will be divided by clkdiv
// clkdiv: how much clock division is required
// bits: the number of bits in the counter register (16 or 32), prescaler is always considered 16 bit
class TimerArrayControl{
public:
    TimerArrayControl(TIM_HandleTypeDef *const htim, const uint32_t fclk=F_CPU, const uint32_t clkdiv=F_CPU/100'000, const uint32_t bits=16);

    void begin(); // start interrupt generation for the listeners
    void attachTimer(Timer* timer); // add a timer to the array, when it fires, the callback function is called
    void detachTimer(Timer* timer); // remove a timer from the array, stopping the callback event
    void changeTimerPeriod(Timer* timer, uint32_t period); // change the period of the timer without changing the start time

    const uint32_t fclk;
    const uint32_t clkdiv;
    const uint8_t bits;

    static const auto TARGET_CC_CHANNEL = TIM_CHANNEL_1;
    static const auto TARGET_CCIG_FLAG = TIM_EGR_CC1G;
    static const uint8_t prescaler_bits = 16;
    static const auto max_prescale = (1 << prescaler_bits);

    const uint32_t max_count = (1 << bits) - 1;
    const uint32_t prediv = // select the appropriate predivision, clkdiv will be altered accordingly
        (clkdiv > max_prescale) ? (
            ((clkdiv >> 1) > max_prescale) ? (
                4
            ) : (
                2
            )
        ) : (
            1
        );
    
    // divide clkdiv, to find out the actual prescaler value, then divide fclk with prescaler and prediv
    const uint32_t prescaler = clkdiv/prediv > max_prescale ? max_prescale : clkdiv/prediv;
    const uint32_t fcnt = fclk/prescaler/prediv; // counting frequency

protected:
    void tick();
    void registerAttachedTimer(uint32_t cnt);
    void registerDetachedTimer();
    void registerPeriodChange();

    TAC_CallbackChain tickCallback;
    TIM_HandleTypeDef *const htim;
    Timer timerString;

    // only one timer and operation flag needed, the request variable has to be atomic rw
    static const uint8_t REQUEST_NONE = 0;
    static const uint8_t REQUEST_ATTACH = 1;
    static const uint8_t REQUEST_DETACH = 2;
    static const uint8_t REQUEST_PERIOD_CHANGE = 3;
    volatile uint8_t request;
    Timer* requestTimer;
    uint32_t requestPeriod;
    volatile bool isTickOngoing;
};




