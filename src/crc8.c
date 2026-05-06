
#include "crc8.h"
#include <stdint.h>


/**
 * @brief Calculate checksum (CRC-8) according to ATM standard.
 * @note Polynomial: 0x07 (x^8 + x^2 + x + 1). Initial value: 0x00
 */
 
inline uint8_t crc8_atm(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00;
    
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i]; // XOR next data byte
        
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                // If most significant bit is 1, shift and XOR with polynomial
                crc = (crc << 1) ^ 0x07;
            } else {
                // Otherwise just shift
                crc <<= 1;
            }
        }
    }
    
    return crc;
}