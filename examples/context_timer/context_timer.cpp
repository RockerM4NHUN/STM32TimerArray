#include "app.h"

// include pin naming
#include "main.h"

// include setup timer handles
#include "tim.h"

#include "STM32TimerArray.hpp"

// The same setup as in blinky example, with Pin and differnt app_start. 
uint32_t timerInputFrequency = F_CPU;
uint32_t frequencyDivision = timerInputFrequency/10000;
uint32_t timerCounterBits = 16;
TimerArrayControl control(
    &htim2, // handle for the used timer hardware, setup by CubeMX
    timerInputFrequency,
    frequencyDivision,
    timerCounterBits);


// Dummy Pin implementation to showcase context timer usage
class Pin{
    GPIO_TypeDef* port;
    uint32_t pin;
    TimerArrayControl& control;
    ContextTimer<Pin> timer;
    
    // we always need a static callback function
    static void timer_callback(Pin* pin){
        pin->off(); // turn off pin after delay
    }

public:
    Pin(GPIO_TypeDef* port, uint32_t pin, TimerArrayControl& control, uint32_t t_on) :
        port(port),
        pin(pin),
        control(control),
        timer(t_on, false, this, timer_callback) // pass the constructed Pin object to the timer
    {}

    void toggle(){HAL_GPIO_TogglePin(port, pin);}
    void on(){HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);}
    void off(){HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);}
    
    void flash(){ // flash the timer once, for t_on time
        // if timer is not running
        if (!timer.isRunning()){
            on(); // turn on pin
            control.attachTimer(&timer); // set timeout for turning it off
        }
    }
};


void app_start(){

    // setup pin object for user LED
    Pin led(
        LD2_GPIO_Port, // port
        LD2_Pin,       // pin
        control, // timer array to use
        500     // on time ticks in 100us quanta representing 50ms
    );

    // Start timer array configured to run at 10 kHz (tick frequency).
    control.begin();

    // flash the LED every second
    while(1){
        led.flash();
        HAL_Delay(1000);
    }
}