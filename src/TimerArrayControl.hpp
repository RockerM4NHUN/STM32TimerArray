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
// delay: ticks of timer array controller until firing
// isPeriodic: does the timer restart immedietely when fires
// f: static function called when timer is firing
class Timer{
public:
    using callback_function = void(*)();
    Timer(const callback_function f);
    Timer(uint32_t delay, bool isPeriodic, const callback_function f);
    
    bool isRunning();

    // Changing the timers delay will not affect the current firing event, only the next one.
    // To restart the timer with the new delay, detach and attach it.
    // To change the timer's delay without restart, use the changeTimerDelay method
    // in the TimerArrayControl.
    uint32_t delay; // required delay of timer (in ticks)
    bool isPeriodic; // should the timer be immedietely restarted after firing

protected:
    uint32_t target; // counter value that the timer fires at next
    void *const f; // WARNING: unsafe if you force the call of a certain fire method instead of letting the inheritance decide
    bool running;
    Timer* next;

    virtual void fire();

    friend class TimerArrayControl;
};

// Represents a Timer with context.
//
// delay: ticks of timer array controller until firing
// isPeriodic: does the timer restart immedietely when fires
// ctx: context pointer to an object, will be passed to the callback function
// ctxf: static function called when timer is firing, takes a Context* pointer argument
template<typename Context>
class ContextTimer : public Timer{
public:
    using dynamic_callback_function = void(*)(Context*);
    ContextTimer(Context* ctx, const dynamic_callback_function ctxf) : ctx(ctx), Timer(ctxf) {}
    ContextTimer(uint32_t delay, bool isPeriodic, Context* ctx, const dynamic_callback_function ctxf) : Timer(delay, isPeriodic, (callback_function)ctxf), ctx(ctx) {}
protected:
    Context* ctx;

    virtual void fire(){
        ((dynamic_callback_function)f)(ctx);
    }
};


// Implements timer controller for hardware handling,
// it encapsulates any hardware related issue and presents a simple common API.
// Requires a timer with capture compare capabilities (implementing a timer array
// with only a resetting counter requires significantly more computations).
// Sets 1, 2 or 4 additional clock division if necessary.
// By default has 1 kHz tick speed, a 1 ms delay needs a tick value of 100.
// 
// fclk: timer's input clock speed, will be divided by clkdiv
// clkdiv: how much clock division is required
// bits: the number of bits in the counter register (16 or 32), prescaler is always considered 16 bit
class TimerArrayControl : TAC_CallbackChain{
public:
    TimerArrayControl(TIM_HandleTypeDef *const htim, const uint32_t fclk=F_CPU, const uint32_t clkdiv=F_CPU/1000, const uint32_t bits=16);

    void begin(); // start interrupt generation for the listeners
    void stop(); // halt the hardware timer, stop interrupt generation
    void attachTimer(Timer* timer); // add a timer to the array, when it fires, the callback function is called
    void detachTimer(Timer* timer); // remove a timer from the array, stopping the callback event
    void changeTimerDelay(Timer* timer, uint32_t delay); // change the delay of the timer without changing the start time
    void attachTimerInSync(Timer* timer, Timer* reference); // add timer to the array, like it was attached the same time as the reference timer
    uint32_t remainingTicks(Timer* timer) const;

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
    void registerAttachedTimer(uint32_t cnt, Timer* timer);
    void registerDetachedTimer(Timer* timer);
    void registerDelayChange(uint32_t cnt, Timer* timer, uint32_t delay);
    void registerAttachedTimerInSync(uint32_t cnt, Timer* timer, Timer* reference);

    void f(TIM_HandleTypeDef*);

    TIM_HandleTypeDef *const htim;
    Timer timerString;

    // only one timer and operation flag needed, the request variable has to be atomic rw
    static const uint8_t REQUEST_NONE = 0;
    static const uint8_t REQUEST_ATTACH = 1;
    static const uint8_t REQUEST_DETACH = 2;
    static const uint8_t REQUEST_DELAY_CHANGE = 3;
    static const uint8_t REQUEST_ATTACH_SYNC = 4;
    volatile uint8_t request;
    Timer* requestTimer;
    Timer* requestReferenceTimer;
    uint32_t requestDelay;
    volatile bool isTickOngoing;
};




