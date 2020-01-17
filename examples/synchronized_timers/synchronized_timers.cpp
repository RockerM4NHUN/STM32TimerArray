#include "app.h"

// include pin naming
#include "main.h"

// include setup timer handles
#include "tim.h"

#include "STM32TimerArray.hpp"

uint32_t timerInputFrequency = F_CPU;
uint32_t frequencyDivision = timerInputFrequency/10000;
uint32_t timerCounterBits = 16;
TimerArrayControl control(
    &htim2, // handle for the used timer hardware, setup by CubeMX
    timerInputFrequency,
    frequencyDivision,
    timerCounterBits);

// requested states of LED
bool slowState = false;
bool fastState = false;

void updateLED(){
    // set pin if either slow or fast flashin requires it, otherwise turn off
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, (GPIO_PinState)(slowState || fastState));
}

void turn_off_fast(){
    fastState = false;
    updateLED();
}

void turn_off_slow(){
    slowState = false;
    updateLED();
}

Timer t_fast_off(
    500, // 50 ms delay
    false, // one shot timer, when fires, it is automatically detached
    turn_off_fast
);

Timer t_slow_off(
    5000, // 500 ms delay
    false, // one shot timer, when fires, it is automatically detached
    turn_off_slow
);

void fast_flash_led(){
    control.attachTimer(&t_fast_off);
    fastState = true;
    updateLED();
}

void slow_flash_led(){
    control.attachTimer(&t_slow_off);
    slowState = true;
    updateLED();
}

Timer t_fast_flash(
    10000,    // the number of ticks to wait (1 sec)
    true,     // isPeriodic, false means one shot, true means repeating
    fast_flash_led // pointer to the static callback function
);

Timer t_slow_flash(
    10000,    // the number of ticks to wait (1 sec)
    false,    // isPeriodic, false means one shot, true means repeating
    slow_flash_led // pointer to the static callback function
);

void app_start(){

    // Attaching timers when the controller is stopped will create a synchronized
    // state. Timers will fire in a deterministic manner until they get detached,
    // or they get manually fired either by the user or with delay changes.
    // To recreate this kind of synchrony a timer can be attached with the
    // attachTimerInSync(timer, reference) function. The effect is the same as if
    // timer was attached in the same tick as reference. This functionality is
    // especially useful, when the hardware timer ticks faster than the interrupt
    // handler could finish. If a timer attaches another timer in it's callback
    // and synchronization is required, attachTimerInSync will solve the problem.
    // If a new event has to be synchronized with an ongoing periodic sequence,
    // attachTimerInSync solves the problem again.

    // Start the hardware timer and fast flashing.
    control.begin();
    control.attachTimer(&t_fast_flash);

    // Watch the user pushbutton if a manual fire is requested
    bool previous_value = 0;
    while(1){
         // read the pushbutton, on NUCLEO-F303RE it is inverted,
         // check your board if this is the case, if not, remove negation
        bool button = !HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
        
        // on rising edge (when button is pushed down)
        if (previous_value == 0 && button == 1){

            // initiate a single slow flash
            if (!t_slow_flash.isRunning()){
                // Attach in sync will start a timer (t_slow_flash) as if it was started
                // at the same time as the reference (t_fast_flash). The actual time of
                // the button push does not matter, t_slow_flash will be always in sync
                // with t_fast_flash.
                control.attachTimerInSync(&t_slow_flash, &t_fast_flash);
            }

            // debouncing delay for the button, just to be safe
            control.sleep(100);
        }

        // don't forget to store value
        previous_value = button;
    }
}