#ifndef HAL_LAMP_H
#define HAL_LAMP_H

#include <stdint.h>
#include <stdbool.h>

// Initialize the GPIO pins for all 4 lamps
void HAL_Lamp_Init(void);

// Set the direct physical state of a specific lamp (Index: 0 to 3)
void HAL_Lamp_SetState(uint8_t lampIndex, bool state);

// Toggle the physical state of a specific lamp
void HAL_Lamp_Toggle(uint8_t lampIndex);

#endif // HAL_LAMP_H