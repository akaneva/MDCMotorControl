#include "hal_lamp.h"
#include "stm32f4xx_hal.h"

#define NUM_LAMPS 2

// 1. Define a structure to hold the arbitrary Port and Pin for each lamp
typedef struct {
    GPIO_TypeDef* port;
    uint16_t      pin;
} LampHardwareMap_t;

// 2. Map your arbitrary ports and pins here! 
// This acts as your hardware configuration table.
static const LampHardwareMap_t LampMap[NUM_LAMPS] = {
    {GPIOE, GPIO_PIN_5},   // Lamp 0: e.g., On-board LED on PA5
    {GPIOE, GPIO_PIN_5}  // Lamp 1:
};

void HAL_Lamp_Init(void) {
    // 3. Enable clocks for all ports used in the LampMap.
    // It's safe to call these multiple times, but make sure 
    // every port used in the array above is enabled here.
    __HAL_RCC_GPIOE_CLK_ENABLE();
    //__HAL_RCC_GPIOB_CLK_ENABLE();
    //__HAL_RCC_GPIOC_CLK_ENABLE();
    //__HAL_RCC_GPIOD_CLK_ENABLE();

    // 4. Initialize each pin dynamically based on the configuration table
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    for (uint8_t i = 0; i < NUM_LAMPS; i++) {
        GPIO_InitStruct.Pin = LampMap[i].pin;
        
        // Initialize the specific port and pin
        HAL_GPIO_Init(LampMap[i].port, &GPIO_InitStruct);
        
        // Turn off the lamp by default
        HAL_GPIO_WritePin(LampMap[i].port, LampMap[i].pin, GPIO_PIN_RESET);
    }
}

void HAL_Lamp_SetState(uint8_t lampIndex, bool state) {
    if (lampIndex >= NUM_LAMPS) return; // Boundary check

    // Use the mapping table to control the exact arbitrary port and pin
    if (state) {
        HAL_GPIO_WritePin(LampMap[lampIndex].port, LampMap[lampIndex].pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(LampMap[lampIndex].port, LampMap[lampIndex].pin, GPIO_PIN_RESET);
    }
}

void HAL_Lamp_Toggle(uint8_t lampIndex) {
    if (lampIndex >= NUM_LAMPS) return; // Boundary check
    
    HAL_GPIO_TogglePin(LampMap[lampIndex].port, LampMap[lampIndex].pin);
}