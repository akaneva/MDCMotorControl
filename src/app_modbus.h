#ifndef APP_MODBUS_H
#define APP_MODBUS_H

#include <stdint.h>
#include <stdbool.h>

/* ==============================================================================
 * MODBUS TCP SERVER MODULE
 * ==============================================================================
 * This module implements a basic Modbus TCP server listening on port 502.
 * It uses the LwIP Raw API (Callback-based) to ensure non-blocking operation,
 * which is critical for maintaining smooth stepper motor ramps.
 * ============================================================================== */

// --- READ-ONLY TELEMETRY REGISTERS ---
#define REG_ENCODER_POS        100  // uint16_t: 0 - 14399
#define REG_MOTOR_STATE        101  // uint16_t: 0=Stopped, 1=Accel, 2=Cruise, 3=Decel
#define REG_CYCLE_TIME         102  // uint16_t: Last revolution time in milliseconds
#define REG_CYCLE_COUNTER      103  // uint16_t: Heartbeat counter (increments every Z-pulse) 

// --- WRITE/READ CONTROL REGISTERS (MOTOR) ---
#define REG_MOTOR_CTRL         200  // uint16_t: 0=Stop, 1=Start
#define REG_MOTOR_DIR          201  // uint16_t: 0=Forward, 1=Reverse
#define REG_MOTOR_RPM          202  // uint16_t: RPM * 10 (e.g. 155 = 15.5 RPM)
#define REG_MOTOR_ACCEL        203  // uint16_t: Seconds * 10 (e.g. 50 = 5.0 sec)
#define REG_MOTOR_MICROSTEPS   204  // Microsteps register (0 - 65535)

// --- WRITE/READ CONTROL REGISTERS (ENCODER & TELEMETRY) ---
#define REG_STREAM_CTRL        300  // uint16_t: 0=Stop Stream, 1=Start Stream
#define REG_ENCODER_DIR        301  // uint16_t: 0=Normal, 1=Reversed
#define REG_UART_BAUD          302  // uint16_t: BaudRate / 100 (e.g. 10000 = 1Mbps)
#define REG_AUTO_STREAM        303  // uint16_t: 0=Disable AutoStart, 1=Enable AutoStart

// Network Settings Registers (New)
#define REG_IP_HIGH            310  // Bytes 0.1 
#define REG_IP_LOW             311  // Bytes 2.3
#define REG_NETMASK_HIGH       312
#define REG_NETMASK_LOW        313
#define REG_GATEWAY_HIGH       314
#define REG_GATEWAY_LOW        315


#define REG_SAVE_SETTINGS      400  // uint16_t: Write 1 to save current config to Flash
#define REG_SYSTEM_REBOOT      401


// Data structure to bridge Modbus network requests with main loop execution
typedef struct {
    // Control Flags
    bool pendingMotorStart;
    bool pendingMotorStop;     
    bool pendingStreamStart;   
    bool pendingStreamStop;   
    bool pendingEncoderDir;    
    bool pendingBaudChange;   
    bool pendingSaveCmd;      
    bool pendingRebootCmd;    
    
    bool     encoderReversed;  
    bool     streamActive;     
    bool     autoStartStream;  
    uint8_t  motorDirection;  
    uint16_t microsteps;       
    uint32_t uartBaudRate;    

    uint8_t  ipAddress[4];     
    uint8_t  netmask[4];       
    uint8_t  gateway[4];       
    float    targetRPM;       
    float    accelTimeSec;     
} ModbusConfig_t;

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the LwIP Modbus PCB and callbacks
void App_Modbus_Init(void);

// Returns a pointer to the global configuration struct for the main loop to process
ModbusConfig_t* App_Modbus_GetConfig(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MODBUS_H */