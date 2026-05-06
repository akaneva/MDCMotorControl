#ifndef HAL_SWITCH_H
#define HAL_SWITCH_H

#include <stdint.h>
#include <stdbool.h>

// Initialize the GPIO pin for the mechanical switch
void HAL_Switch_Init(void);

// Read the immediate, un-debounced physical state of the switch
// Returns true if the switch is in the ON position, false if OFF
bool HAL_Switch_GetRawState(void);

#endif // HAL_SWITCH_H