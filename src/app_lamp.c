#include "app_lamp.h"
#include "hal_lamp.h"

#define NUM_LAMPS 2

// Blinking intervals in milliseconds
#define BLINK_SLOW_INTERVAL_MS 500 
#define BLINK_FAST_INTERVAL_MS 100 

// Track the requested mode for each lamp
static LampMode_t currentModes[NUM_LAMPS];

// Track the time elapsed for each lamp's blinking logic
static uint16_t timeCounters[NUM_LAMPS];

void App_Lamp_Init(void) {
    HAL_Lamp_Init();

    // Initialize all software states to OFF
    for (uint8_t i = 0; i < NUM_LAMPS; i++) {
        currentModes[i] = LAMP_MODE_OFF;
        timeCounters[i] = 0;
    }
}

void App_Lamp_SetMode(uint8_t lampIndex, LampMode_t mode) {
    if (lampIndex >= NUM_LAMPS) return;

    // Only reset the timer and state if the mode is actually changing
    if (currentModes[lampIndex] != mode) {
        currentModes[lampIndex] = mode;
        timeCounters[lampIndex] = 0;

        // Immediately apply static states
        if (mode == LAMP_MODE_OFF) {
            HAL_Lamp_SetState(lampIndex, false);
        } else if (mode == LAMP_MODE_ON) {
            HAL_Lamp_SetState(lampIndex, true);
        }
    }
}

void App_Lamp_Tick_1ms(void) {
    // Process the non-blocking blink logic for all lamps
    for (uint8_t i = 0; i < NUM_LAMPS; i++) {
        
        if (currentModes[i] == LAMP_MODE_BLINK_SLOW) {
            timeCounters[i]++;
            if (timeCounters[i] >= BLINK_SLOW_INTERVAL_MS) {
                timeCounters[i] = 0;
                HAL_Lamp_Toggle(i);
            }
        } 
        else if (currentModes[i] == LAMP_MODE_BLINK_FAST) {
            timeCounters[i]++;
            if (timeCounters[i] >= BLINK_FAST_INTERVAL_MS) {
                timeCounters[i] = 0;
                HAL_Lamp_Toggle(i);
            }
        }
        // LAMP_MODE_ON and LAMP_MODE_OFF require no periodic updates
    }
}