#include "hwsetup.h"

// -----                  -----
// ----- Hardware handles -----
// -----                  -----
TIM_HandleTypeDef htim2;


// -----                  -----
// ----- Helper functions -----
// -----                  -----

// implement systick to actually tick
void SysTick_Handler(void) {
    HAL_IncTick();
}

void TIM2_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim2);
}

void Error_Handler(void) {
    while(1);
}

void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /**Initializes the CPU, AHB and APB busses clocks 
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }
    /**Initializes the CPU, AHB and APB busses clocks 
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_TIM2;
    PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
    PeriphClkInit.Tim2ClockSelection = RCC_TIM2CLK_HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}

// -----                                                   -----
// ----- Setup functions for different hardware components -----
// -----                                                   -----

void hwsetup_init(){
    HAL_Init();
    SystemClock_Config();
    HAL_Delay(2000);
}







void hwsetup_internal_timing(){

    /*********************************/
    /* TIM2 for interrupt generation */
    /*********************************/

    __HAL_RCC_TIM2_CLK_ENABLE();
    
    /* TIM2 interrupt Init */
    HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    /* TIM2 peripheral pre-init */
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    // set to default, should change later
    htim2.Init.Prescaler = 0;
    htim2.Init.Period = 0;

    // set to config used by internal timing tests
    htim2.Instance = TIM2;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
        Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }

    // disable CCx channels
    HAL_TIM_OC_Stop_IT(&htim2, TIM_CHANNEL_1);
    HAL_TIM_OC_Stop_IT(&htim2, TIM_CHANNEL_2);
    HAL_TIM_OC_Stop_IT(&htim2, TIM_CHANNEL_3);
    HAL_TIM_OC_Stop_IT(&htim2, TIM_CHANNEL_4);
}

void hwteardown_internal_timing(){
    __HAL_RCC_TIM2_CLK_DISABLE();

    __HAL_TIM_DISABLE_IT(&htim2, TIM_IT_UPDATE|TIM_IT_CC1|TIM_IT_CC2|TIM_IT_CC3|TIM_IT_CC4|TIM_IT_BREAK|TIM_IT_TRIGGER);
    __HAL_TIM_CLEAR_IT(&htim2, TIM_SR_UIF|TIM_SR_CC1IF|TIM_SR_CC2IF|TIM_SR_CC3IF|TIM_SR_CC4IF|TIM_SR_BIF|TIM_SR_TIF);

    /* TIM2 interrupt Deinit */
    HAL_NVIC_DisableIRQ(TIM2_IRQn);
}
