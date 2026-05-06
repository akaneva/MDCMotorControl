#ifndef APP_ENCODER_H
#define APP_ENCODER_H

#include <stdint.h>
#include <stdbool.h>


// Initialize encoder with the specified number of pulses per revolution (PPR)
void App_Encoder_Init( void );

// Return antenna position in degrees (0.00 - 359.99)
// float App_Encoder_GetAngle( void );

// Return absolute position in pulses for one revolution (e.g. 0 - 14399)
// Used specifically for fast telemetry
uint16_t App_Encoder_GetPosition( void );

/**
 * @brief  Sets the logical counting direction of the encoder.
 * @param  reversed true to reverse counting direction, false for normal.
 */
void App_Encoder_SetDirection(bool reversed);
/**
 * @brief Gets the cycle time of the encoder.
 * 
 * @return uint16_t - Time in milliseconds for one full revolution (Z-pulse to Z-pulse). Capped at 65535 ms to fit in a Modbus register.
 */
uint16_t App_Encoder_GetCycleTime(void);
void App_Encoder_ResetCycleTime(void);
 

#endif // APP_ENCODER_H