#include "app_switch.h"
#include "hal_switch.h"

// Time in milliseconds the contact must be stable to register a valid state
#define DEBOUNCE_TIME_MS 50 

static bool switchStableState = false;
static bool switchLastRawState = false;
static uint16_t debounceCounter = 0;

void App_Switch_Init(void) {
    HAL_Switch_Init();
    
    // Read initial state so we don't trigger a false toggle on boot
    switchStableState = HAL_Switch_GetRawState();
    switchLastRawState = switchStableState;
}

void App_Switch_Tick_1ms(void) {
    bool rawState = HAL_Switch_GetRawState();

    // If the raw state changed (due to bounce or human interaction)
    if (rawState != switchLastRawState) {
        debounceCounter = 0;             // Reset the stability timer
        switchLastRawState = rawState;   // Track the new raw state
    } else {
        // State is stable, increment the timer
        if (debounceCounter < DEBOUNCE_TIME_MS) {
            debounceCounter++;
            
            // If the state has been stable for exactly DEBOUNCE_TIME_MS
            if (debounceCounter == DEBOUNCE_TIME_MS) {
                // Lock in the new stable state
                switchStableState = rawState;
            }
        }
    }
}

bool App_Switch_GetState(void) {
    return switchStableState;
}