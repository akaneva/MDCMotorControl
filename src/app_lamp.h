#ifndef APP_LAMP_H
#define APP_LAMP_H

#include <stdint.h>

// Define possible logical behaviors for the lamps
typedef enum {
    LAMP_MODE_OFF = 0,
    LAMP_MODE_ON = 1,
    LAMP_MODE_BLINK_SLOW = 2, // e.g., 1 Hz (500ms ON, 500ms OFF)
    LAMP_MODE_BLINK_FAST = 3  // e.g., 5 Hz (100ms ON, 100ms OFF)
} LampMode_t;

// Initialize the lamp application module
void App_Lamp_Init(void);

// Non-blocking timing routine. MUST be called every 1 millisecond.
void App_Lamp_Tick_1ms(void);

// Set the behavior of a specific lamp (Index: 0 to 3)
void App_Lamp_SetMode(uint8_t lampIndex, LampMode_t mode);

#endif // APP_LAMP_H