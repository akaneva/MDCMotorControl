#ifndef HAL_ENCODER_H
#define HAL_ENCODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


// Function pointer for the Z-pulse (Zero) callback
// This function will be triggered on the rising edge of the Z-pulse, indicating a full revolution.
typedef void (*EncoderZeroCallback_t)(void);

// Define a function pointer type for the encoder step event.
// This function will be triggered on every valid hardware pulse.
typedef void (*EncoderStepCallback_t)(uint16_t currentPosition);


// Initialize the hardware timer for Quadrature Encoder Interface (QEI)
void HAL_Encoder_Init(void);

// Initialize the Z-pulse input pin and attach the hardware EXTI interrupt
void HAL_Encoder_Init_ZeroInput(EncoderZeroCallback_t callback);

// Subscribe to high-speed encoder step events.
// Param callback: Pointer to the application-level function handling the step.
void HAL_Encoder_Init_StepCallback(EncoderStepCallback_t callback);

// Set the hardware counting direction of the quadrature encoder.
// Param direction: true = Normal (Forward), false = Reversed (Backward)
void HAL_Encoder_SetCountDirection(bool direction);

// Read the absolute hardware counter value
int32_t HAL_Encoder_GetRawCount(void);

// Enable hardware interrupts for encoder steps (starts fast telemetry)
void HAL_Encoder_EnableInterrupts(void);

// Disable hardware interrupts for encoder steps (stops fast telemetry)
void HAL_Encoder_DisableInterrupts(void);

// Reset the hardware counter to zero
void HAL_Encoder_Reset(void);


void HAL_Encoder_TimerHires_Init(void);
uint32_t HAL_Encoder_GetHiresTimeUs(void);

#endif 