#include "hal_motor.h"
// #include "app_motor.h"
#include "stm32f4xx_hal.h"

// ==============================================================================
// HARDWARE PINOUT MAPPING (STM32F407)
// ==============================================================================
#define STEP_PORT GPIOB             // Step pin (PB10) - Configured as Alternate Function linked to TIM2_CH3
#define STEP_PIN  GPIO_PIN_10 

#define DIR_PORT  GPIOA            // Direction pin (PA6) - Configured as Open-Drain based on your hardware requirements
#define DIR_PIN   GPIO_PIN_6

#define EN_PORT   GPIOB             // Enable pin (PB5) - Configured as Open-Drain based on your hardware requirements
#define EN_PIN    GPIO_PIN_5

// ==============================================================================
// TIMER SETTINGS & VARIABLES
// ==============================================================================
TIM_HandleTypeDef htim2;

// The APB1 timer clock frequency. For STM32F407 at 168MHz, APB1 timers run at 84MHz.
// We will set the prescaler to 83, making the timer counter run at exactly 1 MHz (1 tick = 1 uS).

#define TIM2_BASE_FREQ_HZ 84000000.0
#define TIM2_PRESCALER    (0)



void HAL_Motor_Init(void) {
    // 1. Enable Peripheral Clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();

    // 2. Configure Direction (PA5) and Enable (PB5) pins
    // Configured as Open-Drain based on your previous hardware requirements 
    // (typical when sinking current for industrial stepper driver optocouplers).
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    GPIO_InitStruct.Pin = DIR_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DIR_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = EN_PIN;
    HAL_GPIO_Init(EN_PORT, &GPIO_InitStruct);

    // Default states (Motor disabled, DIR = 0)
    HAL_Motor_Enable(false);
    HAL_Motor_SetDirection(0);

    // 3. Configure STEP Pin (PA6) for Timer Alternate Function
    // Configured as Open-Drain AF2 to link directly to TIM2_CH3
     GPIO_InitStruct = (GPIO_InitTypeDef){0};
    
    GPIO_InitStruct.Pin = STEP_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD; 
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    //FOR TEST >>
    // GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; 

    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(STEP_PORT, &GPIO_InitStruct);

    // 4. Configure TIM2 Base
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = TIM2_PRESCALER; 
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period =0x1000;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE; // Glitch-free frequency updates
    HAL_TIM_PWM_Init(&htim2);

    // 5. Configure TIM3 Channel 1 for PWM Generation
    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0; // 50% duty cycle
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    // sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3);
}

void HAL_Motor_SetFrequency( uint32_t freqHz) {
    if (freqHz == 0) return; 

    uint32_t new_arr = (TIM2_BASE_FREQ_HZ / freqHz) - 1;
    TIM2->ARR = new_arr;
    TIM2->CCR3 = (new_arr + 1) / 2;
}

void HAL_Motor_Enable(bool state) {
    // Assuming active LOW logic (true  = enabled, false = disabled)

    if (state) {
        HAL_GPIO_WritePin(EN_PORT, EN_PIN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(EN_PORT, EN_PIN, GPIO_PIN_RESET);
    }
}

void HAL_Motor_SetDirection(uint8_t dir) {
    HAL_GPIO_WritePin(DIR_PORT, DIR_PIN, dir ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void HAL_Motor_StartTimer(void) {

    TIM2->EGR = TIM_EGR_UG;  //reload regisers 

    // Enable the Timer Counter and PWM Output
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);

}

void HAL_Motor_StopTimer(void) {
    // Stop the Timer Counter and PWM Output
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
}
