#ifndef APP_TELEMETRY_H
#define APP_TELEMETRY_H

#include <stdint.h>

// Initialize protocol and hardware serial port
void App_Telemetry_Init(uint32_t baud);
void App_Telemetry_StartStream(void);
void App_Telemetry_StopStream(void);
void App_Telemetry_SetBaudRate(uint32_t newBaudRate);

#endif // APP_TELEMETRY_H
