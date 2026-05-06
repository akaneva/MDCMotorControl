#ifndef APP_SWITCH_H
#define APP_SWITCH_H

#include <stdbool.h>

// Initialize the switch application module
void App_Switch_Init(void);

// Non-blocking debounce routine. MUST be called every 1 millisecond.
void App_Switch_Tick_1ms(void);

// Get the current stable logical state of the switch
// Returns true if stable ON, false if stable OFF
bool App_Switch_GetState(void);

#endif // APP_SWITCH_H