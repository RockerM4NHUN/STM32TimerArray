#include "app.h"

// include pin naming
#include "main.h"

// include setup timer handles
#include "tim.h"

#include "STM32TimerArray.hpp"

// The same setup as in blinky example, with different app_start and timers. 

uint32_t timerInputFrequency = F_CPU;
uint32_t frequencyDivision = timerInputFrequency/10000;
uint32_t timerCounterBits = 16;
TimerArrayControl control(
    &htim2, // handle for the used timer hardware, setup by CubeMX
    timerInputFrequency,
    frequencyDivision,
    timerCounterBits);


void turn_off_led(){
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
}

Timer t_off(
    500, // 50 ms delay
    false, // one shot timer, when fires, it is automatically detached
    turn_off_led
);

// count the number of flashes
uint8_t flashes = 0;

// signal if the fire was due to a button push
bool buttonPushFlag = false;

void flash_led(){

    // if this callback is result of a manual fire request, reset flash count
    if (buttonPushFlag){
        buttonPushFlag = false;
        flashes = 0;
    }

    // turn on LED
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);

    // attach timer for turning off the LED (reattach if it was already running)
    if (t_off.isRunning()) {
        control.detachTimer(&t_off);
    }

    // when timer is detached, direct delay change is allowed
    t_off.delay(flashes == 0 ? 5000 : 500);

    // increment flashes, reset when it would reach 5
    flashes = flashes == 4 ? 0 : (flashes + 1);

    control.attachTimer(&t_off);
}

Timer t_flash(
    10000,    // the number of ticks to wait (1 sec)
    true,     // isPeriodic, false means one shot, true means repeating
    flash_led // pointer to the static callback function
);

void app_start(){

    // Start the hardware timer and user timer
    control.begin();
    control.attachTimer(&t_flash);

    // Immedietely fire timer to see a flash at start
    control.manualFire(&t_flash);

    // Watch the user pushbutton if a manual fire is requested
    bool previous_value = 0;
    while(1){
         // read the pushbutton, on NUCLEO-F303RE it is inverted,
         // check your board if this is the case, if not, remove negation
        bool button = !HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
        
        // on rising edge (when button is pushed down)
        if (previous_value == 0 && button == 1){
            
            // signal a button push
            // Can't reset flashes directly, since the callback runs in an interrupt
            // and simultaneous access of a variable can cause erroneous states.
            // Setting and resetting a bool is fine if the base thread not reads it.
            buttonPushFlag = true;

            // initiate flash
            control.manualFire(&t_flash);

            // debouncing delay for the button, just to be safe
            control.sleep(100);
        }

        // don't forget to store value
        previous_value = button;
    }
}