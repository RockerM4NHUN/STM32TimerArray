#include "hwsetup.h"

// -----                  -----
// ----- Helper functions -----
// -----                  -----

// implement systick to actually tick
void SysTick_Handler(void) {
    HAL_IncTick();
}

// -----                                                   -----
// ----- Setup functions for different hardware components -----
// -----                                                   -----

void hwsetup_init(){
    HAL_Init();
    HAL_Delay(2000);
}