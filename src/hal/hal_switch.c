#include "hal_switch.h"
#include "stm32f4xx_hal.h"

// Define your switch hardware pin here
#define SWITCH_PORT GPIOE
#define SWITCH_PIN  GPIO_PIN_4

void HAL_Switch_Init(void) {
    // 1. Enable GPIO Clock
    __HAL_RCC_GPIOE_CLK_ENABLE();

    // 2. Configure GPIO Pin as Input with internal Pull-Up resistor
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = SWITCH_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP; 
    HAL_GPIO_Init(SWITCH_PORT, &GPIO_InitStruct);
}

bool HAL_Switch_GetRawState(void) {
    // Assuming Active-LOW configuration (Switch connects pin to GND when ON).
    // Because of the PULLUP, the pin reads HIGH (RESET = 0) when the switch is ON.
    if (HAL_GPIO_ReadPin(SWITCH_PORT, SWITCH_PIN) == GPIO_PIN_RESET) {
        return true;  // Switch is ON
    }
    return false;     // Switch is OFF
}