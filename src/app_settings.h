#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <stdint.h>
#include <stdbool.h>

// Use Sector 11 (128 Kbytes) base address for STM32F40x (1MB Flash versions)
#define FLASH_SETTINGS_ADDR     0x080E0000 
#define SETTINGS_MAGIC_WORD     0xAA55AA55

/**
 * @brief Configuration structure stored in Flash memory.
 *        Aligned to 32-bit boundaries for direct hardware CRC calculation.
 */
typedef struct {
    uint32_t magicWord;       // Verification word
    float    targetRPM;       // Target motor speed
    float    accelTimeSec;    // Acceleration ramp time
    uint32_t motorDirection;  // 0 = Forward, 1 = Reverse (uint32_t for alignment)
    uint32_t encoderReversed; // 0 = Normal, 1 = Reversed (uint32_t for alignment)
    uint32_t uartBaudRate;    // Telemetry UART Baud Rate
    uint32_t autoStartStream; // 0 = Disabled, 1 = Enabled
    uint32_t microsteps;      // driver microsteps parameter 
    uint8_t  ipAddress[4];    // Static IP Address (e.g., 192.168.1.151)
    uint8_t  netmask[4];      // Subnet Mask (e.g., 255.255.255.0)
    uint8_t  gateway[4];      // Default Gateway (e.g., 192.168.1.1)
    uint32_t crc32;           // Hardware computed CRC32 (MUST be the last element)
} __attribute__((packed, aligned(4))) SystemSettings_t;

// ==============================================================================
// C / C++ LINKAGE COMPATIBILITY
// Prevent C++ compiler from mangling function names so they can be called from C
// ==============================================================================
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Initializes the settings module and loads configuration from Flash.
 *         If the Flash is empty or the CRC32 verification fails, 
 *         it loads factory defaults and formats the Flash sector.
 * @retval None
 */
void App_Settings_Init(void);

/**
 * @brief  Saves the provided configuration structure into the Flash memory.
 *         It automatically computes and appends the hardware CRC32.
 * @param  newSettings: Pointer to the new configuration structure to be saved.
 * @retval true if the Flash erase and write operations were successful, false otherwise.
 */
bool App_Settings_Save(const SystemSettings_t* newSettings);

/**
 * @brief  Retrieves a pointer to the currently active RAM copy of the settings.
 *         Used by other modules to read the system configuration.
 * @retval Pointer to the active SystemSettings_t structure.
 */
SystemSettings_t* App_Settings_Get(void);

#ifdef __cplusplus
}
#endif

#endif // APP_SETTINGS_H