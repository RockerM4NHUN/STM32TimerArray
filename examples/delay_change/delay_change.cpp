#include "app.h"

// include pin naming
#include "main.h"

// include setup timer handles
#include "tim.h"

#include "STM32TimerArray.hpp"

// The same setup as in blinky example, with different app_start. 

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
        // speed up timer every 100 milliseconds until delay is less than 15 milliseconds (150/10000 seconds)
        while(t_toggle.delay > 150){
            HAL_Delay(100);
            
            // set the new delay to 90% of the previous one

            // change timer delay in the traditional way
            // Notice that if the timer is reattached more often than the delay,
            // the timer will never fire! Try uncommenting this code and see for yourself.
            /* Pros:
             *  - Simple to implement and understand.
             *  - In some circumstances works fine.
             * Cons:
             *  - If the change happens quicker than the timer frequency the timer will never fire.
             *  - The above is also true for delay lengthening.
             *  - Depending on execution speed ruins timer synchrony.
             */
            // control.detachTimer(&t_toggle);
            // control.changeTimerDelay(&t_toggle, (t_toggle.delay*9)/10); // change delay offline
            // control.attachTimer(&t_toggle);

            // change timer delay in the correct way, online
            /* Pros:
             *  - Handles delay change in both directions correctly, no longer delay between fires
             *    than the maximum of the old and new delays, no shorter than the minimum of them.
             *  - The frequency transient is smoother than leaving the range of old and new delays.
             * Cons:
             *  - Possibly ruins timer synchrony (if have to fire immedietely).
             *  - Slightly more complicated.
             */
            control.changeTimerDelay(&t_toggle, (t_toggle.delay*9)/10);
            // If the affected timer is attached, the delay either changes in a synchronous way or
            // fires and restarts immedietely. The time between two fires will have a transient
            // when the delay is changed but the actual number of ticks will be between the old
            // delay and the new one. On delay shortening the timer might be fired immedietely or
            // restarted synchronously.
        }
        
        // slow down timer every 100 milliseconds until delay is more than 500 milliseconds
        while(t_toggle.delay < 5000){
            HAL_Delay(100);
            
            // set the new delay to 111% of the previous one
            control.changeTimerDelay(&t_toggle, (t_toggle.delay*10)/9);
            // Changing delay in the correct way will work for lengthening too.
        }
    }
}
