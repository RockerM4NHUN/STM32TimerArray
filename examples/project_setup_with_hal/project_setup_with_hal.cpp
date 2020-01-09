#include "app.h"
#include "STM32TimerArray.hpp"

#include "stm32_hal.h"

// if this compiles and the LED blinks, the library is included successfully
const char* version_string = STM32TimerArray::version;

void app_start(){
	
    // implementation here ...
	
	// HAL blink example (if LD2 is defined as the user LED, for example in 'stm32_hal.h')
	// for STM32TimerArray based implementation see the blinky example
	while(1){
		HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
		HAL_Delay(500);
	}
}