#include "stm32f4xx_hal.h"
#include "hal_motor.h"
#include "app_motor.h"


#include <stddef.h> // For NULL

// ==============================================================================
// DEFAULT PHYSICAL SETTINGS
// ==============================================================================
//#define DEFAULT_GEAR_RATIO      3.0f    // Gearbox 1:3
//FOR TEST ONLY, USE 1:1 RATIO TO VERIFY THE KINEMATIC ENGINE WITHOUT MECHANICAL LOAD
#define DEFAULT_GEAR_RATIO      3.0f    // Gearbox 1:1
#define DEFAULT_MICROSTEPS      5000    // Driver switches
#define DEFAULT_MIN_MICROSTEPS_CL86HB  800 //  SH-CL86BH minimum  PULSE/REV
#define DEFAULT_MIN_START_RPM   2.0f    // RPM to overcome static friction
#define DEFAULT_ZERO_RPM_0  0.0f 
#define DEFAULT_MAX_RPM    200.0f

#define TRAPEZOID_AVG_DIVISOR   2.0f

#define SECONDS_PER_MINUTE 60.0f
#define MS_PER_SECOND      1000.0f
// ==============================================================================
// STATIC VARIABLES (Private to this file)
// ==============================================================================

// Initial configuration for the radar antenna
static ShaftProfile_t shaft = {
    .microsteps  = DEFAULT_MICROSTEPS,
    .gearRatio   = DEFAULT_GEAR_RATIO,
    .minStartRPM = DEFAULT_MIN_START_RPM,
    .targetRPM   = 0.0f,
    .startFreq   = 0.0f,
    .targetFreq  = 0.0f,
    .accelStepPerMs = 0.0f
};

// Volatile keyword is CRITICAL here because these variables are modified 
// inside the Hardware Interrupt (App_Motor_Tick) and read by the main loop.
static volatile MotorState_t currentState = STATE_STOPPED;
static volatile float currentFreq = 0.0f;    

// ==============================================================================
// INITIALIZATION
// ==============================================================================
void App_Motor_Init(void) {
    // Initialize the physical timer and GPIOs
    HAL_Motor_Init();  
}

// ==============================================================================
// PROFILE CALCULATION (Runs in Main Context)
// ==============================================================================

void App_Motor_CalculateProfile(float targetRPM, float accelTimeSec , uint16_t microsteps ) {

    //Update hardware microsteps only if the value is valid
    if (microsteps >= DEFAULT_MIN_MICROSTEPS_CL86HB) {
        shaft.microsteps = microsteps;
    }
    //Safety checks to prevent math errors or zero division
    if (targetRPM <= shaft.minStartRPM) {
        targetRPM = shaft.minStartRPM + 0.1f; 
    }
    if (accelTimeSec <= 0.001f) {
        accelTimeSec = 0.001f; 
    }
    // safety check 
    
    if((targetRPM > DEFAULT_ZERO_RPM_0) && (targetRPM < DEFAULT_MAX_RPM) )
    {
        shaft.targetRPM = (float)targetRPM;
    }
    //Calculate start frequency (Hz)
    // Formula: (RPM * GearRatio) / 60 = Rotations per second at the motor shaft
    // Freq (Hz) = RPS * Microsteps
    float startRPS = (shaft.minStartRPM * shaft.gearRatio) / SECONDS_PER_MINUTE;
    shaft.startFreq = (startRPS * (float)shaft.microsteps);

    // 3. Calculate target frequency (Hz)
    float targetRPS = (shaft.targetRPM * shaft.gearRatio) / SECONDS_PER_MINUTE;
    shaft.targetFreq = (targetRPS * (float)shaft.microsteps);

    // 4. Calculate linear acceleration per 1 millisecond (SysTick base)
    // How much frequency to add per 1ms to reach the target frequency in exactly 'accelTimeSec'
    float deltaFreqHz = (float)(shaft.targetFreq - shaft.startFreq);
    float totalTimeMs = accelTimeSec * MS_PER_SECOND;
    
    shaft.accelStepPerMs = deltaFreqHz / totalTimeMs;
}

// ==============================================================================
// EXECUTION: Start and Stop Commands (Runs in Main Context)
// ==============================================================================
void App_Motor_Start(uint8_t direction) {
    if (currentState == STATE_STOPPED || currentState == STATE_DECELERATING) {
        
        HAL_Motor_SetDirection(direction);

        HAL_Motor_Enable(true);
                 
        currentFreq = (float)shaft.startFreq;
        currentState = STATE_ACCELERATING;
                  
        HAL_Motor_SetFrequency(shaft.startFreq);
        
        HAL_Motor_StartTimer();
    }
}

void App_Motor_Stop(void) {
    if (currentState == STATE_ACCELERATING || currentState == STATE_CRUISING) {
        currentState = STATE_DECELERATING;
        
        // Ensure the timer interrupt is active to process the deceleration slope
        // HAL_Motor_EnableInterrupt(true); 
    }
}

MotorState_t App_Motor_GetState(void) {
    return currentState;
}

// REAL TIME KINEMATICS ENGINE: Hardware Interrupt (ISR Context)
// ==============================================================================
// WARNING: This function executes thousands of times per second. 
// Keep it extremely fast. Avoid complex math or blocking calls here.

void App_Motor_KinematicTick_1ms(void) {

    if (currentState == STATE_ACCELERATING) {
        // Linear acceleration: Velocity increases constantly over TIME, not distance
        currentFreq += shaft.accelStepPerMs; 
                           
        if (currentFreq >= shaft.targetFreq) {
            currentFreq = shaft.targetFreq;
            currentState = STATE_CRUISING;
        }
        
        // Push the new target frequency to the hardware timer (TIM3)
        HAL_Motor_SetFrequency((uint32_t)currentFreq);
    }
    else if (currentState == STATE_DECELERATING) {
        currentFreq -= shaft.accelStepPerMs;
                           
        if (currentFreq <= shaft.startFreq) {
            // Motion complete. Shut down hardware gracefully.
            HAL_Motor_StopTimer();
            HAL_Motor_Enable(false);
            currentState = STATE_STOPPED;
        } else {
            // Push the decelerated frequency to the hardware
            HAL_Motor_SetFrequency((uint32_t)currentFreq);
        }
    }
}
/**
 * @brief 
 * 
 * @param microsteps 
 */
void App_Motor_SetMicrosteps(const uint16_t  microsteps) 
{
    if (microsteps > 0) {
        shaft.microsteps = microsteps;
    }
}
    
