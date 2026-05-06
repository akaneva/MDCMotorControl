#ifndef HAL_TELEMETRY_H
#define HAL_TELEMETRY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Define a function pointer type for the RX callback.
// This function will be triggered immediately upon receiving a single byte.
typedef void (*UartRxCallback_t)(uint8_t byte);

// Initialize the hardware UART peripheral.
// Param baud: The baud rate (e.g., 1000000 for 1 Mbps)
// Param callback: Pointer to the function handling incoming bytes
/**
 * @brief Initialize the telemetry hardware peripheral
 * @param baud - Baud rate for the UART
 * @param callback - Pointer to the application-level receive callback that will be called immediately when a byte is received.
 * @note        This allows the application to process incoming data in real-time without polling.
 */
void HAL_Telemetry_Init(uint32_t baud, UartRxCallback_t callback);

/**
 * @brief Set a new baud rate for the telemetry UART. This function will gracefully stop the current UART,
 *        reconfigure it with the new baud rate, and restart it.
 * @param newBaudRate 
 * @return * void 
 */
void HAL_Telemetry_SetBaudRate(uint32_t newBaudRate);

/**
 * @brief Write a buffer of data to the telemetry UART
 * @param buffer Pointer to the data buffer to be sent
 * @param length Length of the data buffer in bytes
 * @return true if the transmission was successfully initiated, false otherwise
 */
bool HAL_Telemetry_WriteBuffer(const uint8_t *buffer, size_t length);

#endif // HAL_TELEMETRY_H