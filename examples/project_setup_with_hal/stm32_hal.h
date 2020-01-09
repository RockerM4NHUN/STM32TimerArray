// stm32 hal binding for libs used by STM32 HAL project

// Use a microcontroller from the stm32f3xx family, change it according to your board
#include "stm32f3xx_hal.h"

// Set user LED to PA5, change it according to your board
#define LD2_GPIO_Port   GPIOA
#define LD2_Pin         GPIO_PIN_5
