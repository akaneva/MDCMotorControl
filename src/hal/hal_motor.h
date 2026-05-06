#ifndef HAL_MOTOR_H
#define HAL_MOTOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void HAL_Motor_Init(); // Initialize pins and timer

void HAL_Motor_Enable(bool state); // Enable/Disable driver (Enable)

void HAL_Motor_SetDirection(uint8_t dir); // Set direction (DIR)

void HAL_Motor_SetFrequency(uint32_t freqHz); // Set frequency and maintain 50% PWM automatically


void HAL_Motor_StartTimer(); // Start the timer and PWM output

void HAL_Motor_StopTimer(); // Stop the timer and PWM output

#endif // HAL_MOTOR_H
