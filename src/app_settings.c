#include "app_settings.h"
#include "stm32f4xx_hal.h"
#include "SEGGER_RTT.h"
#include <string.h>

static SystemSettings_t activeSettings;
static CRC_HandleTypeDef hcrc;

// Default factory settings to be used if Flash is empty or corrupt
static const SystemSettings_t defaultSettings = {
    .magicWord       = SETTINGS_MAGIC_WORD,
    .targetRPM       = 8.0f,
    .accelTimeSec    = 5.0f,
    .motorDirection  = 0,
    .encoderReversed = 0,
    .uartBaudRate    = 230400,
    .autoStartStream = 0,
    .microsteps      = 5000,   
    .ipAddress       = {192, 168, 1, 151},
    .netmask         = {255, 255, 255, 0},
    .gateway         = {192, 168, 1, 1},
    .crc32           = 0 // Will be calculated dynamically before saving
};

/**
 * @brief  Initializes the STM32 Hardware CRC32 Peripheral.
 *         Enables the clock and prepares the CRC unit for calculations.
 * @retval None
 */
static void InitHardwareCRC(void) {
    __HAL_RCC_CRC_CLK_ENABLE();
    hcrc.Instance = CRC;
    
    if (HAL_CRC_Init(&hcrc) != HAL_OK) {
        SEGGER_RTT_WriteString(0, "Hardware CRC Init Failed!\n");
    }
}

/**
 * @brief  Calculates the CRC32 checksum of the settings structure.
 *         It calculates over all parameters EXCEPT the last 'crc32' field itself.
 * @param  settings: Pointer to the settings structure to be evaluated.
 * @retval Computed 32-bit CRC value.
 */
static uint32_t CalculateSettingsCRC(const SystemSettings_t* settings) {
    // Calculate the number of 32-bit words excluding the last 'crc32' field
    uint32_t payloadLengthWords = (sizeof(SystemSettings_t) - sizeof(uint32_t)) / 4;
    
    // Reset the hardware CRC calculation unit for a fresh calculation
    __HAL_CRC_DR_RESET(&hcrc);
    
    // Calculate and return the CRC32 using the HAL driver
    return HAL_CRC_Calculate(&hcrc, (uint32_t*)settings, payloadLengthWords);
}

/**
 * @brief  Initializes the settings module and loads configuration from Flash.
 *         If the Flash is empty or the CRC32 verification fails, 
 *         it loads factory defaults and formats the Flash sector.
 * @retval None
 */
void App_Settings_Init(void) {
    InitHardwareCRC();

    SystemSettings_t* flashMemory = (SystemSettings_t*)FLASH_SETTINGS_ADDR;

    // 1. Verify if the sector contains our Magic Word
    if (flashMemory->magicWord == SETTINGS_MAGIC_WORD) {
        
        // 2. Verify Data Integrity via Hardware CRC32
        uint32_t computedCRC = CalculateSettingsCRC(flashMemory);
        
        if (computedCRC == flashMemory->crc32) {
            memcpy(&activeSettings, flashMemory, sizeof(SystemSettings_t));
            SEGGER_RTT_WriteString(0, "Settings loaded and CRC32 verified successfully.\n");
            return;
        } else {
            SEGGER_RTT_WriteString(0, "Settings CRC32 MISMATCH! Flash data is corrupted.\n");
        }
    } else {
        SEGGER_RTT_WriteString(0, "No valid settings found in Flash (Empty or Invalid).\n");
    }

    // Fallback: Load factory defaults into active RAM
    memcpy(&activeSettings, &defaultSettings, sizeof(SystemSettings_t));
    SEGGER_RTT_WriteString(0, "Loaded factory default settings.\n");
    
    // Automatically repair the Flash memory by saving the default settings
    App_Settings_Save(&activeSettings);
}

/**
 * @brief  Saves the provided configuration structure into the Flash memory.
 *         It automatically computes and appends the hardware CRC32.
 * @param  newSettings: Pointer to the new configuration structure to be saved.
 * @retval true if the Flash erase and write operations were successful, false otherwise.
 */
bool App_Settings_Save(const SystemSettings_t* newSettings) {
    HAL_StatusTypeDef status;
    uint32_t sectorError = 0;
    
    // Create a local copy to inject the freshly computed CRC32
    SystemSettings_t dataToWrite;
    memcpy(&dataToWrite, newSettings, sizeof(SystemSettings_t));
    dataToWrite.magicWord = SETTINGS_MAGIC_WORD;
    dataToWrite.crc32 = CalculateSettingsCRC(&dataToWrite);

    // Calculate total 32-bit words to flash
    uint32_t wordsToWrite = sizeof(SystemSettings_t) / 4;
    uint32_t* dataPtr = (uint32_t*)&dataToWrite;

    SEGGER_RTT_WriteString(0, "Erasing Flash Sector 11...\n");

    // Unlock the Flash control register access
    HAL_FLASH_Unlock();

    // Configure the erase operation for Sector 11
    FLASH_EraseInitTypeDef eraseInitStruct;
    eraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
    eraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3; // 2.7V to 3.6V range
    eraseInitStruct.Sector       = FLASH_SECTOR_11;
    eraseInitStruct.NbSectors    = 1;

    // Execute the erase operation
    status = HAL_FLASHEx_Erase(&eraseInitStruct, &sectorError);
    if (status != HAL_OK) {
        HAL_FLASH_Lock();
        SEGGER_RTT_WriteString(0, "Flash erase FAILED!\n");
        return false;
    }

    SEGGER_RTT_printf(0, "Writing new settings with CRC32...0x%08X\n", dataToWrite.crc32);

    // Write the new data word by word
    for (uint32_t i = 0; i < wordsToWrite; i++) {
        uint32_t targetAddress = FLASH_SETTINGS_ADDR + (i * 4);
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, targetAddress, dataPtr[i]);
        
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            SEGGER_RTT_WriteString(0, "Flash write FAILED!\n");
            return false;
        }
    }

    // Lock the Flash control register to prevent accidental modifications
    HAL_FLASH_Lock();
    
    // Update the active RAM copy to reflect the successful save
    memcpy(&activeSettings, &dataToWrite, sizeof(SystemSettings_t));
    
    SEGGER_RTT_WriteString(0, "Settings saved successfully.\n");
    return true;  
}

/**
 * @brief  Retrieves a pointer to the currently active RAM copy of the settings.
 *         Used by other modules to read the system configuration.
 * @retval Pointer to the active SystemSettings_t structure.
 */
SystemSettings_t* App_Settings_Get(void) {
    return &activeSettings;
}