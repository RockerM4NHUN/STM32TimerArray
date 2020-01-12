#include "app.h"

// include pin naming
#include "main.h"

// include setup timer handles
#include "tim.h"

#include "STM32TimerArray.hpp"

// The same setup as in blinky example, with differnt app_start. 

uint32_t timerInputFrequency = F_CPU;
uint32_t timerCounterBits = 16;
uint32_t frequencyDivision = timerInputFrequency/10000;
TimerArrayControl control(
    &htim2, // handle for the used timer hardware, setup by CubeMX
    timerInputFrequency,
    frequencyDivision,
    timerCounterBits);

void toggle_led(){
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
}

Timer t_toggle(
    5000,      // the number of ticks to wait
    true,      // isPeriodic, false means one shot, true means repeating
    toggle_led // pointer to the static callback function
);

void app_start(){

    // change delay for t_toggle, before attaching
    // when timer is not attached, the delay simply changes and nothing special happens
    control.changeTimerDelay(&t_toggle, 5000);

    // start the timer and the actual interrupt generation
    control.attachTimer(&t_toggle);
    control.begin();

    while(1){
        // speed up timer every 250 milliseconds until delay is less than 15 milliseconds (150/10000 seconds)
        while(t_toggle.delay > 150){
            HAL_Delay(250);
            
            // set the new delay to 90% of the previous one

            // change timer delay in the traditional way
            // Notice that if the timer is reattached more often than the delay,
            // the timer will never fire!
            // Try uncommenting this code and see for yourself.
            // control.detachTimer(&t_toggle);
            // control.changeTimerDelay(&t_toggle, (t_toggle.delay*9)/10); // delay change for detached timer is not special
            // control.attachTimer(&t_toggle);

            // change timer delay in a synchronous way, good enough for some blinking LEDs
            control.changeTimerDelay(&t_toggle, (t_toggle.delay*9)/10);
            // If the affected timer is attached, the delay changes in a synchronous way.
            // The target is modified as if the timer delay was the new amount originally.
            // This has some caveats though.
            // Callbacks will be potentially skipped if the delay is shortened.
            // For example in a case when the timer was started at 0 ticks
            // and should fire at 110 ticks, the delay will be changed to 99 ticks.
            // If this happens between 99 and 110, the new target will be in the past.
            // The library handles this by skipping the already passed target
            // and continuing as normal with the next timing period. To see why
            // this is the chosen way of handling, consider a case when the new
            // delay is only 10 ticks. How many missed callbacks should we fire?
            // The best decision is to let the user choose. See the 'advanced_delay_change'
            // example for different handling strategies.
        }
        
        // slow down timer every 250 milliseconds until delay is more than 500 milliseconds
        while(t_toggle.delay < 5000){
            HAL_Delay(250);
            
            // set the new delay to 111% of the previous one
            control.changeTimerDelay(&t_toggle, (t_toggle.delay*10)/9);
            // In this case the synchronous delay change works fine. Since the
            // new delay is greater than the original it can be moved a bit
            // further without problem.
        }
    }
}
