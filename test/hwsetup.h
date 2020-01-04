
#ifndef HW_SETUP_H
#define HW_SETUP_H

#include "stm32_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

// ----- Hardware handles -----
extern TIM_HandleTypeDef htim2;

// ----- Setup functions for different hardware components -----
void hwsetup_init();

void hwsetup_internal_timing();
void hwteardown_internal_timing();


#ifdef __cplusplus
}
#endif

#endif // HW_SETUP_H
