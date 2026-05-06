#ifndef CRC8_H
#define CRC8_H

#include <stdint.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Calculate checksum (CRC-8) according to ATM standard.
 * Polynomial: 0x07 (x^8 + x^2 + x + 1)
 * Initial value: 0x00
 */
uint8_t crc8_atm(const uint8_t *data, uint8_t len);
 

#endif // CRC8_H