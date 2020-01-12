#include "app.h"

// include pin naming
#include "main.h"

// include setup timer handles
#include "tim.h"

#include "STM32TimerArray.hpp"

// The same setup as in delay change example, with differnt app_start and change_delay.

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


void change_delay(uint32_t new_delay){
    /*
     * Attempt #1
     * Pros:
     *  - Simple to implement and understand.
     *  - In some circumstances works fine.
     * Cons:
     *  - If the change happens quicker than the timer frequency, the timer will never fire.
     *  - The above is also true for delay lengthening.
     *  - Depending on execution speed ruins timer synchrony.
     */
    // control.detachTimer(&t_toggle);
    // // delay change for detached timer is not special
    // control.changeTimerDelay(&t_toggle, new_delay);
    // control.attachTimer(&t_toggle);

    /*
     * Attempt #2
     * Pros:
     *  - Already implemented.
     *  - Works properly on delay lengthening.
     *  - Keeps timer synchrony.
     * Cons:
     *  - Can skip fire events, causing glitches. Timer will never fire on specific delay
     *    change patterns. Note the occasional glitch when the blinking speeds up.
     *  - Not as simple as attempt #1.
     */
    // control.changeTimerDelay(&t_toggle, new_delay);

    /*
     * Attempt #3 (probably the best way)
     * Pros:
     *  - Handles delay change in both directions correctly, no longer delay between fires
     *    than the maximum of the old and new delays.
     *  - The frequency transient is smoother than attempt #1 and #2.
     * Cons:
     *  - Ruins timer synchrony.
     *  - Slightly more complicated.
     */
    uint32_t elapsed = control.elapsedTicks(&t_toggle);
    // if according to the new delay the timer should have been fired, fire it immedietely
    if (elapsed > new_delay){
        control.manualFire(&t_toggle); // firing will ruin timer synchrony
    }
    control.changeTimerDelay(&t_toggle, new_delay);

    /*
     * Attempt #4 (manualFireInSync is not implemented, but the concept is shown)
     * If there is a reasonable need, implementation is possible.
     * Pros:
     *  - Works properly on delay lengthening.
     *  - The frequency transient is smoother than attempt #1 and #2.
     *  - Keeps timer synchrony.
     * Cons:
     *  - Can fire more rapidly than the minimum of the old and new delays.
     *  - Slightly more complicated.
     */
    // uint32_t elapsed = control.elapsedTicks(&t_toggle);
    // // if according to the new delay the timer should have been fired, fire it immedietely
    // if (elapsed > new_delay){
    //     control.manualFireInSync(&t_toggle); // not atcually implemented, until there is a need
    // }
    // control.changeTimerDelay(&t_toggle, new_delay);
}


void app_start(){

    // start the timer and the actual interrupt generation
    control.attachTimer(&t_toggle);
    control.begin();

    while(1){
        // speed up timer every 100 milliseconds until delay is less than 15 milliseconds
        // (150/10000 seconds)
        while(t_toggle.delay > 150){
            HAL_Delay(100);
            
            // set the new delay to 90% of the previous one
            uint32_t new_delay = (t_toggle.delay*9)/10;
            change_delay(new_delay);
        }
        
        // slow down timer every 100 milliseconds until delay is more than 500 milliseconds
        while(t_toggle.delay < 5000){
            HAL_Delay(100);
            
            // set the new delay to 111% of the previous one
            uint32_t new_delay = (t_toggle.delay*10)/9;
            change_delay(new_delay);
        }
    }
}
