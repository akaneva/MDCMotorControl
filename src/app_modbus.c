#include "app_modbus.h"
#include "lwip/tcp.h"
#include "app_motor.h"
#include "app_encoder.h"
#include "app_settings.h"  
#include <string.h>

#define MODBUS_PORT            502
#define MBAP_HEADER_SIZE       7
#define FC_READ_HOLDING_REGS   0x03
#define FC_WRITE_SINGLE_REG    0x06

/**
 * @brief Global state holding parameters and execution flags.
 *        Marked as volatile because it is accessed by the LwIP callback
 *        and read by the main loop.
 */
static volatile ModbusConfig_t ModbusCfg= {
    // Control Flags
    .pendingMotorStart   = false,
    .pendingMotorStop    = false,
    .pendingStreamStart  = false,
    .pendingStreamStop   = false,
    .pendingEncoderDir   = false,
    .pendingBaudChange   = false,
    .pendingSaveCmd      = false,
    .pendingRebootCmd    = false,
    
    .encoderReversed = false,
    .streamActive    = false,// Active Parameters
    .autoStartStream = false,
    .motorDirection  = 0,
    .microsteps      = 5000,
    .uartBaudRate    = 230400,

    .ipAddress    = {192, 168, 1, 151},
    .netmask      = {255, 255, 255, 0},
    .gateway     = {192, 168, 1, 1},
    .targetRPM       = 10.0f,
    .accelTimeSec    = 2.0f
};


// --- PRIVATE PROTOTYPES ---
static err_t modbus_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t modbus_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void  modbus_err_cb(void *arg, err_t err);
static void  modbus_close_conn(struct tcp_pcb *tpcb);

/**
 * @brief  Retrieves the pointer to the global Modbus configuration structure.
 *         This allows the main loop to safely read and clear pending flags.
 * @retval Pointer to the ModbusConfig_t structure.
 */
ModbusConfig_t* App_Modbus_GetConfig(void) {
    // Cast away volatile for main loop usage
    return (ModbusConfig_t*)&ModbusCfg; 
}

/**
 * @brief  Initializes the Modbus TCP server.
 *         Creates a new TCP Protocol Control Block (PCB), binds it to 
 *         port 502, and starts listening for incoming client connections.
 */
void App_Modbus_Init(void) {
    struct tcp_pcb *modbus_pcb = tcp_new();
    
    if (modbus_pcb != NULL) {
        err_t err = tcp_bind(modbus_pcb, IP_ANY_TYPE, MODBUS_PORT);
        if (err == ERR_OK) {
            modbus_pcb = tcp_listen(modbus_pcb);
            tcp_accept(modbus_pcb, modbus_accept_cb);
        } else {
            tcp_close(modbus_pcb);
        }
    }
}

/**
 * @brief  Callback executed when a new TCP connection is accepted.
 * @param  arg User-supplied argument (unused).
 * @param  newpcb The TCP Protocol Control Block for the new connection.
 * @param  err Any error associated with the acceptance.
 * @retval ERR_OK on success.
 */
static err_t modbus_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);
    
    // Setup the receive and error callbacks for the new connection
    tcp_recv(newpcb, modbus_recv_cb);
    tcp_err(newpcb, modbus_err_cb);
    
    // Enable Keep-Alive to detect dead or disconnected clients
    newpcb->so_options |= SOF_KEEPALIVE;
    
    return ERR_OK;
}

/**
 * @brief  Callback executed when a TCP packet is received.
 *         Parses the MBAP header and Modbus function codes (FC03, FC06).
 * @param  arg User-supplied argument (unused).
 * @param  tpcb The TCP Protocol Control Block of the connection.
 * @param  p The pbuf containing the received data.
 * @param  err Any receive error.
 * @retval ERR_OK on success, or an LwIP error code.
 */
static err_t modbus_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    // If pbuf is NULL, the client has closed the connection
    if (p == NULL) {
        modbus_close_conn(tpcb);
        return ERR_OK;
    }
    
    // If there is an LwIP error, free the buffer and return the error
    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }
    
    // Acknowledge the received data to the TCP stack
    tcp_recved(tpcb, p->tot_len);
    
    // Verify that the packet has at least the size of an MBAP header
    if (p->len >= MBAP_HEADER_SIZE) {
        uint8_t *data = (uint8_t *)p->payload;
        uint8_t functionCode = data[7];
        
        // ==========================================================
        // FC 03: Read Holding Registers
        // ==========================================================
        if (functionCode == FC_READ_HOLDING_REGS) {
            uint16_t startAddress = (data[8] << 8) | data[9];
            uint16_t quantity = (data[10] << 8) | data[11];
            
            // Limit quantity to prevent buffer overflow (Max 20 registers per request)
            if (quantity > 0 && quantity <= 20) {
                uint8_t byteCount = quantity * 2;
                uint8_t res[64]; // Max size needed is 9 + (20 * 2) = 49 bytes
                
                // Copy MBAP Transaction + Protocol IDs
                memcpy(res, data, 6);       
                
                uint16_t length = 3 + byteCount; // UnitID(1) + FC(1) + ByteCount(1) + Data
                res[4] = (length >> 8) & 0xFF;
                res[5] = length & 0xFF;
                res[6] = data[6];           // Unit ID
                res[7] = functionCode;
                res[8] = byteCount;
                
                // Populate requested registers
                for (uint16_t i = 0; i < quantity; i++) {
                    uint16_t addr = startAddress + i;
                    uint16_t val = 0;
                    
                    // Map the requested address to the physical/logical values
                    switch (addr) {
                        case REG_ENCODER_POS:       val = App_Encoder_GetPosition(); break;
                        case REG_MOTOR_STATE:       val = (uint16_t)App_Motor_GetState(); break;
                        case REG_MOTOR_CTRL:        val = (App_Motor_GetState() == STATE_STOPPED) ? 0 : 1; break;
                        case REG_CYCLE_TIME:        val = App_Encoder_GetCycleTime(); break;
                        case REG_CYCLE_COUNTER:     val = App_Encoder_GetCycleCounter(); break; 
                        case REG_MOTOR_DIR:         val = ModbusCfg.motorDirection; break;
                        case REG_MOTOR_RPM:         val = (uint16_t)(ModbusCfg.targetRPM * 10.0f); break;
                        case REG_MOTOR_ACCEL:       val = (uint16_t)(ModbusCfg.accelTimeSec * 10.0f); break;
                        case REG_MOTOR_MICROSTEPS:  val = (uint16_t)ModbusCfg.microsteps; break;
                        case REG_STREAM_CTRL:       val = ModbusCfg.streamActive ? 1 : 0; break;
                        case REG_ENCODER_DIR:       val = ModbusCfg.encoderReversed ? 1 : 0; break;
                        case REG_UART_BAUD:         val = (uint16_t)(ModbusCfg.uartBaudRate / 100); break;
                        case REG_AUTO_STREAM:       val = ModbusCfg.autoStartStream ? 1 : 0; break;
                        case REG_IP_HIGH:           val = (App_Settings_Get()->ipAddress[0] << 8) | App_Settings_Get()->ipAddress[1];   break;
                        case REG_IP_LOW:            val = (App_Settings_Get()->ipAddress[2] << 8) | App_Settings_Get()->ipAddress[3];    break;
                        case REG_NETMASK_HIGH:      val = (App_Settings_Get()->netmask[0] << 8) | App_Settings_Get()->netmask[1];  break;
                        case REG_NETMASK_LOW:       val = (App_Settings_Get()->netmask[2] << 8) | App_Settings_Get()->netmask[3];      break;
                        case REG_GATEWAY_HIGH:      val = (App_Settings_Get()->gateway[0] << 8) | App_Settings_Get()->gateway[1];  break;                          
                        case REG_GATEWAY_LOW:       val = (App_Settings_Get()->gateway[2] << 8) | App_Settings_Get()->gateway[3];  break;
                        default:                    val = 0; break; // Unmapped registers return 0
                    }
                    
                    // Pack the 16-bit value into the byte array (Big-Endian)
                    res[9 + (i * 2)] = (val >> 8) & 0xFF;
                    res[10 + (i * 2)] = val & 0xFF;
                }
                
                // Transmit the response
                tcp_write(tpcb, res, 9 + byteCount, TCP_WRITE_FLAG_COPY);
            }
        }
        // ==========================================================
        // FC 06: Write Single Register
        // ==========================================================
        else if (functionCode == FC_WRITE_SINGLE_REG) {
            uint16_t regAddress = (data[8] << 8) | data[9];
            uint16_t regValue = (data[10] << 8) | data[11];
            
            // Map the write request to the internal configuration state
            switch (regAddress) {
                case REG_MOTOR_CTRL:
                    if (regValue == 1) ModbusCfg.pendingMotorStart = true;
                    else ModbusCfg.pendingMotorStop = true;
                    break;
                case REG_MOTOR_DIR:
                    ModbusCfg.motorDirection = (regValue > 0) ? 1 : 0;
                    break;
                case REG_MOTOR_RPM:
                    ModbusCfg.targetRPM = (float)regValue / 10.0f; // Decode x10 scaler
                    break;
                case REG_MOTOR_ACCEL:
                    ModbusCfg.accelTimeSec = (float)regValue / 10.0f; // Decode x10 scaler
                    break;
                case REG_MOTOR_MICROSTEPS:                      // <--- NEW
                     ModbusCfg.microsteps = (uint32_t)regValue;
                break;
                
                case REG_STREAM_CTRL:
                    if (regValue == 1) ModbusCfg.pendingStreamStart = true;
                    else ModbusCfg.pendingStreamStop = true;
                    break;
                case REG_ENCODER_DIR:
                    ModbusCfg.encoderReversed = (regValue > 0) ? true : false;
                    ModbusCfg.pendingEncoderDir = true;
                    break;
                case REG_UART_BAUD:
                    ModbusCfg.uartBaudRate = (uint32_t)regValue * 100; // Decode /100 scaler
                    ModbusCfg.pendingBaudChange = true;
                    break;
                case REG_AUTO_STREAM:
                    ModbusCfg.autoStartStream = (regValue > 0) ? true : false;
                    break;

                case REG_SAVE_SETTINGS:
                    if (regValue == 1) ModbusCfg.pendingSaveCmd = true;
                    break;
                case REG_IP_HIGH: 
                    ModbusCfg.ipAddress[0] = (regValue >> 8) & 0xFF; 
                    ModbusCfg.ipAddress[1] = regValue & 0xFF;
                break;   
                case REG_IP_LOW:
                    ModbusCfg.ipAddress[2] = (regValue >> 8) & 0xFF;
                    ModbusCfg.ipAddress[3] = regValue & 0xFF;
                break;
                case REG_SYSTEM_REBOOT:
                if (regValue == 1) ModbusCfg.pendingRebootCmd = true;
                break;
            }
            
            // Standard Modbus behavior: Echo the exact request as a response for FC06
            tcp_write(tpcb, data, 12, TCP_WRITE_FLAG_COPY);
        }
        
        // Trigger immediate transmission of any queued data
        tcp_output(tpcb); 
    }
    
    // Free the received pbuf memory
    pbuf_free(p);
    return ERR_OK;
}

/**
 * @brief  Callback executed when a TCP error occurs.
 * @param  arg User-supplied argument (unused).
 * @param  err The LwIP error code.
 */
static void modbus_err_cb(void *arg, err_t err) {
    // Suppress unused warnings. The LwIP stack automatically closes 
    // the PCB on fatal errors, so no explicit cleanup is required here.
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);
}

/**
 * @brief  Gracefully closes a Modbus TCP connection and clears its callbacks.
 * @param  tpcb The TCP Protocol Control Block to close.
 */
static void modbus_close_conn(struct tcp_pcb *tpcb) {
    tcp_arg(tpcb, NULL);
    tcp_sent(tpcb, NULL);
    tcp_recv(tpcb, NULL);
    tcp_close(tpcb);
}