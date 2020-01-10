#include "app.h"
#include "main.h"

#include "STM32TimerArray.hpp"

// if this compiles and the LED blinks, the library is included successfully
const char* version_string = STM32TimerArray::version;

void app_start(){

    // implementation here ...

    // HAL blink example (if LD2 is defined as the user LED)
    // for STM32TimerArray based blink implementation see the blink_with_cubemx example
    while(1){
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        HAL_Delay(500);
    }
}