#include "app_telemetry.h"
#include "hal_telemetry.h"     // Universal driver
#include "hal_encoder.h"
#include "app_motor.h"    // For motor status
#include "app_encoder.h"  // For antenna position
#include "crc8.h"         // For checksum
#include "sys_debug.h"    // SEGGER library

 

// --- PORT AND PROTOCOL SETTINGS ---
/*
Serial2 hardware corresponds to USART2 module
TX2 (Transmission): USART6_TX - Pin3
RX2 (Reception):   USART6_Rx - Pin 4 
*/

#define SYNC_BYTE      0x55
#define REQ_POSITION   'P'

// --- RECEIVER STATES ---

typedef enum RxState { 
    WAIT_SYNC, 
    WAIT_CMD, 
    WAIT_CRC 
}eRxState_t;

static eRxState_t currentState = WAIT_SYNC;
static uint8_t rxCmd = 0;

// --- RESPONSE STRUCTURE ---
// Exactly 5 bytes thanks to __attribute__((packed))
typedef struct ResponsePacket {
    uint8_t  sync;          // 1 byte
    uint16_t rawPosition;   // 2 bytes
    uint8_t  motorState;    // 1 byte
    uint8_t  crc;           // 1 byte
}__attribute__((packed)) ResponsePacket_t;


// Exactly 4 bytes structure for ultra-fast DMA transmission
typedef struct FastStreamPacket {
    uint8_t  sync;          // 1 byte
    uint16_t rawPosition;   // 2 bytes
    uint8_t  crc;           // 1 byte
} __attribute__((packed)) FastStreamPacket_t;

static  FastStreamPacket_t streamPacket;
static  ResponsePacket_t response;

static void App_Telemetry_OnEncoderStep(uint16_t currentPosition);

// ---------------------------------------------------------
// THIS FUNCTION IS CALLED AUTOMATICALLY ON NEW BYTE
// (Runs outside loop)
// ---------------------------------------------------------
static void App_Telemetry_OnByteReceived(uint8_t incomingByte) {
   
    switch (currentState) {
        case WAIT_SYNC:
            if (incomingByte == SYNC_BYTE) {
                currentState = WAIT_CMD;
            }
            break;

        case WAIT_CMD:
            rxCmd = incomingByte;
            currentState = WAIT_CRC;
            break;

        case WAIT_CRC:
            // 1. Check if request CRC is valid
            if (crc8_atm(&rxCmd, 1) == incomingByte) {
                
                // 2. If command is for position
                if (rxCmd == REQ_POSITION) {
                    
                    //SEGGER_RTT_WriteString(0, "Command received: Request Position\n");
                    // Prepare packet
                    response.sync = SYNC_BYTE;
                    uint16_t rawPos = App_Encoder_GetPosition();
                    
                    rawPos = __builtin_bswap16(rawPos); //convert to big-endian from little-endian for transmission
                    response.rawPosition =  rawPos;   
                    response.motorState = (uint8_t)App_Motor_GetState();
                    
                     // Calculate CRC for 3 data bytes (position + status)
                    // Since memory is packed, we can pass it as byte array
                    response.crc = crc8_atm((uint8_t*)&response.rawPosition, 3); 
                    
                    // =========================================================
                    // RESPONSE TRACE (READABLE FORMAT)
                    // =========================================================
                    SEGGER_RTT_printf(0, "TX -> [SYNC: 0x%02X] [POS: %u] [STATE: %u] [CRC: 0x%02X]\n", 
                                      response.sync, 
                                      __builtin_bswap16(rawPos), 
                                      response.motorState, 
                                      response.crc);
                    // Send immediately through HAL layer
                    HAL_Telemetry_WriteBuffer( (uint8_t*)&response, sizeof(response) );
                }
            }
            // Regardless of packet validity, start listening from beginning
            currentState = WAIT_SYNC; 
            break;
    }
}

// ---------------------------------------------------------
// INITIALIZATION
// ---------------------------------------------------------
void App_Telemetry_Init(uint32_t baud) {
    
    HAL_Telemetry_Init(baud, App_Telemetry_OnByteReceived);  // Initialize UART with our custom byte-received callback
   // HAL_Encoder_Init_ZeroInput(App_ZeroPulseCallback);         // Ensure encoder Z-pulse is configured for accurate position tracking
    HAL_Encoder_Init_StepCallback(App_Telemetry_OnEncoderStep);
    currentState = WAIT_SYNC;
}

// ==============================================================================
// STREAM CONTROL FUNCTIONS
// ==============================================================================
void App_Telemetry_StartStream(void) {
    // Enable hardware interrupts for encoder steps (defined in hal_encoder)
    HAL_Encoder_EnableInterrupts(); 
    SEGGER_RTT_WriteString(0, "Fast Telemetry Stream STARTED.\n");
}

void App_Telemetry_StopStream(void) {
    // Disable hardware interrupts for encoder steps
    HAL_Encoder_DisableInterrupts();
    SEGGER_RTT_WriteString(0, "Fast Telemetry Stream STOPPED.\n");
}
// This is a pure application-level function. It handles data packing and transmission.
// It does NOT contain any hardware-specific interrupt logic.
static void App_Telemetry_OnEncoderStep(uint16_t currentPosition) {
    
    // 1. Pack the payload using the position provided by the HAL layer
    streamPacket.sync = SYNC_BYTE; 
    streamPacket.rawPosition = __builtin_bswap16(currentPosition); 
    streamPacket.crc = crc8_atm((uint8_t*)&streamPacket.rawPosition, 2);
    
    // 2. Trigger non-blocking DMA transmission
    HAL_Telemetry_WriteBuffer((uint8_t*)&streamPacket, sizeof(streamPacket));
}

/**
 * @brief Сhange the baud rate of the telemetry UART on-the-fly. This function will gracefully stop the current UART,
 * 
 * @param newBaudRate - Тhe new baud rate to set (e.g., 115200, 230400, 1000000)
 */
void App_Telemetry_SetBaudRate(uint32_t newBaudRate)
{
    HAL_Telemetry_SetBaudRate(newBaudRate);  
}