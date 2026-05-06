#ifndef APP_MOTOR_H
#define APP_MOTOR_H

#include <stdint.h>
#include <stdbool.h>

// ==============================================================================
// TYPE DEFINITIONS
// ==============================================================================



// Define the discrete states of the motion controller
typedef enum {
    STATE_STOPPED= 0,
    STATE_ACCELERATING  =1 ,
    STATE_CRUISING =2 ,
    STATE_DECELERATING = 3, 
    // STATE_HOMING = 4
} MotorState_t;

// Combined structure: Mechanics and Kinematics
typedef struct {
    // 1. Hardware constants
    uint16_t microsteps; 
    float gearRatio;             
    float minStartRPM;           
    
    // 2. Calculated parameters (computed before start)
    float targetRPM;             
    float startFreq;          
    float targetFreq;         
    float accelStepPerMs;
} ShaftProfile_t;

// ==============================================================================
// PUBLIC API
// ==============================================================================

void App_Motor_Init(void);

// Phase 1: Prepare calculations
void App_Motor_CalculateProfile(float targetRPM, float accelTimeSec, uint16_t microsteps);

// Phase 2: Execution (direction: 1 for forward, 0 for reverse)
void App_Motor_Start(uint8_t direction);
void App_Motor_Stop(void);

MotorState_t App_Motor_GetState(void);

/**
 * @brief   Real-time kinematic engine tick. Must be called every 1ms from the system timer interrupt.
 * 
 */
void App_Motor_KinematicTick_1ms(void);



#endif // APP_MOTOR_H