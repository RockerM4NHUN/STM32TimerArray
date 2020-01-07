#pragma once

// enable timer module, the library needs the definitions
#define HAL_TIM_MODULE_ENABLED

// include HAL framework regardless of CPU type, this will include the timer module
#include "stm32_hal.h"

#include "CallbackChain.hpp"


// Callback chain setup for HAL_TIM_OC_DelayElapsedCallback function
struct TIM_OC_DelayElapsed_CallbackChainID{};
using TIM_OC_DelayElapsed_CallbackChain = CallbackChain<TIM_OC_DelayElapsed_CallbackChainID, TIM_HandleTypeDef*>;


// Represents a timer, handled by a TimerArrayControl object.
// Attach it to a controller to receive callbacks.
//
// delay: ticks of timer array controller until firing
// periodic: does the timer restart immedietely when fires
// f: static function called when timer is firing
class Timer{
public:
    using callback_function = void(*)();
    Timer(const callback_function f);
    Timer(uint32_t delay, bool periodic, const callback_function f);
    
    bool isRunning();

    // Changing the timers delay will not affect the current firing event, only the next one.
    // To restart the timer with the new delay, detach and attach it.
    // To change the timer's delay without restart, use the changeTimerDelay method
    // in the TimerArrayControl.
    uint32_t delay; // required delay of timer (in ticks)
    bool periodic; // should the timer be immedietely restarted after firing

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
    ContextTimer(Context* ctx, const dynamic_callback_function ctxf) : Timer((callback_function)ctxf), ctx(ctx) {}
    ContextTimer(uint32_t delay, bool isPeriodic, Context* ctx, const dynamic_callback_function ctxf) : Timer(delay, isPeriodic, (callback_function)ctxf), ctx(ctx) {}
protected:
    Context* ctx;

    virtual void fire(){
        ((dynamic_callback_function)f)(ctx);
    }
};


// Implements timer controller for hardware handling,
// it encapsulates any hardware related issue and presents a simple common API.
// Requires a timer with capture compare capabilities.
// By default has 10 kHz tick speed, a 1 ms delay needs a tick value of 10, 0.5 sec is 5000 ticks.
// 
// fclk: timer's input clock speed, will be divided by clkdiv
// clkdiv: how much clock division is required, maximum allowed value depends on the specific timer's prescale register's size
//         currently limited for every timer to 65536 (16 bit prescale register), it could become a setting if needed
// bits: the number of bits in the counter register (16 or 32)
// prescaler: minimum of 65536 and clkdiv, compare with clkdiv to find out if selected prescale is possible
// fcnt: the actual counting frequency based on the settings and limitations
class TimerArrayControl : TIM_OC_DelayElapsed_CallbackChain{
public:
    TimerArrayControl(TIM_HandleTypeDef *const htim, const uint32_t fclk=F_CPU, const uint32_t clkdiv=F_CPU/10000, const uint8_t bits=16);

    void begin(); // start interrupt generation for the listeners
    void stop(); // halt the hardware timer, stop interrupt generation
    void attachTimer(Timer* timer); // add a timer to the array, when it fires, the callback function is called
    void detachTimer(Timer* timer); // remove a timer from the array, stopping the callback event
    void changeTimerDelay(Timer* timer, uint32_t delay); // change the delay of the timer without changing the start time
    void attachTimerInSync(Timer* timer, Timer* reference); // add timer to the array, like it was attached the same time as the reference timer
    void manualFire(Timer* timer);

    uint32_t remainingTicks(Timer* timer) const;
    uint32_t elapsedTicks(Timer* timer) const;
    float actualTickFrequency() const;

    static const auto TARGET_CC_CHANNEL = TIM_CHANNEL_1;
    static const auto TARGET_CCIG_FLAG = TIM_EGR_CC1G;
    static const uint8_t prescaler_bits = 16;
    static const auto max_prescale = (1 << prescaler_bits);

    const uint32_t fclk;
    const uint32_t clkdiv;
    const uint32_t prescaler = clkdiv > max_prescale ? max_prescale : clkdiv;

protected:
    struct TimerFeed{
        Timer root;
        TIM_HandleTypeDef *const htim;
        const uint8_t bits;
        const uint32_t max_count = (1 << bits) - 1;
        uint32_t cnt; // current value of timer counter (saved to freeze while calculating)

        TimerFeed(TIM_HandleTypeDef *const htim, const uint8_t bits);
        Timer* findTimerInsertionLink(Timer* it, Timer* timer);
        Timer* findTimerInsertionLink(Timer* timer);
        void insertTimer(Timer* it, Timer* timer);
        void insertTimer(Timer* timer);
        void removeTimer(Timer* timer);
        void updateTarget(Timer* timer, uint32_t target);

        // check if target comes sooner than reference if we are at cnt
        bool isSooner(uint32_t target, uint32_t reference);
        
        // calculate the next target while staying in sync with the previous one and the current time
        uint32_t calculateNextFireInSync(uint32_t target, uint32_t delay) const;

        void fetchCounter();
    };

    void tick();
    void registerAttachedTimer(Timer* timer);
    void registerDetachedTimer(Timer* timer);
    void registerDelayChange(Timer* timer, uint32_t delay);
    void registerAttachedTimerInSync(Timer* timer, Timer* reference);
    void registerManualFire(Timer* timer);

    void f(TIM_HandleTypeDef*);

    TimerFeed timerFeed;

    // the request variable has to be atomic RW, bytes achieve that
    enum Request : uint8_t {
        NONE = 0,
        ATTACH,
        DETACH,
        DELAY_CHANGE,
        ATTACH_SYNC,
        MANUAL_FIRE
    };

    volatile Request request;
    Timer* requestTimer;
    Timer* requestReferenceTimer;
    uint32_t requestDelay;
    volatile bool isTickOngoing;
};




