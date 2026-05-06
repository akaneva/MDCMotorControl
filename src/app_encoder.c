#include "stm32f4xx_hal.h"
#include "app_encoder.h"
#include "hal_encoder.h"
#include "SEGGER_RTT.h" // Pure C SEGGER library

// --- REVOLUTION TIMING VARIABLES ---


// Stores the timestamp of the last detected Z-pulse
static uint32_t lastZPulseTick = 0;
static volatile uint16_t lastRevolutionTimeMs = 0;  //Store the last measured cycle time safely
// Ensures we have at least two pulses before calculating duration
static bool isFirstPulse = true;


// Variables to store 32-bit microsecond timestamps
static uint32_t lastZPulseCaptureUs = 0;
// Volatile because it is updated in the ISR and read in the main loop
static volatile uint32_t lastRevolutionTimeUs = 0;


static void DEBUG_PrintZPulseTiming(void) {
    uint32_t duration;
    uint32_t currentTick;
    int32_t rpm_int;
    int32_t rpm_frac;

    currentTick= HAL_GetTick(); // Capture system time[cite: 1]
    
    if (!isFirstPulse) 
    {
        duration = currentTick - lastZPulseTick;

        if (duration > 0) 
        {
            // Cap the duration to 16-bits (65535 ms) to prevent Modbus register overflow
            lastRevolutionTimeMs = (duration > 65535) ? 65535 : (uint16_t)duration;
            // Update timestamp for the next revolution calculation[cite: 1]
            lastZPulseTick = currentTick; 

            // Calculate RPM based on the time between Z-pulses[cite: 1]
            float realRPM = 60000.0f / (float)duration;

            // --- FLOAT TO INTEGER CONVERSION FOR RTT ---
            // SEGGER_RTT_printf usually doesn't support %f to save memory.
            // We split the float into an integer part and a 2-decimal fractional part.
            rpm_int = (int32_t)realRPM; 
            rpm_frac = (int32_t)((realRPM - (float)rpm_int) * 100.0f);
            
            // Ensure the fractional part is always positive for display
           /// if (rpm_frac < 0) rpm_frac = -rpm_frac;

            // Output the diagnostic data using integer formatting (%d.%02d)
           
            SEGGER_RTT_printf(0, "Cycle Time: %u ms | Measured Speed: %d.%02d RPM\n\n", 
                              duration, rpm_int, rpm_frac);
 
        } 
    }
    else 
    {
        // Reference point established on the first pulse[cite: 1]
        isFirstPulse = false; 
    }
    // Update timestamp for the next revolution calculation[cite: 1]
    lastZPulseTick = currentTick; 
    
    
}

/**
 * @brief Gets the cycle time of the encoder in milliseconds.
 *        Maintains compatibility with the Modbus 16-bit register map.
 * @return uint16_t Time in milliseconds for one full revolution.
 */
uint16_t App_Encoder_GetCycleTime(void) {
    // Convert microseconds to milliseconds
    uint32_t timeMs = lastRevolutionTimeUs / 1000;
    
    // Cap the value to prevent 16-bit integer overflow during Modbus transmission
    return (timeMs > 65535) ? 65535 : (uint16_t)timeMs;
}

static void App_Encoder_OnZeroPulse( void ) 
{
   
    // 1. IMMEDIATELY capture the exact hardware time (Zero latency)
    uint32_t currentCaptureUs = HAL_Encoder_GetHiresTimeUs();
    HAL_Encoder_Reset(); // Tell hardware to reset

    // 3. Calculate the duration of the current revolution in microseconds
    if (!isFirstPulse) {
        // This math works flawlessly even if the hardware timer overflows,
        // thanks to the nature of 32-bit unsigned arithmetic!
        uint32_t durationUs = currentCaptureUs - lastZPulseCaptureUs;
        
        // Save the precise duration for the application layer
        lastRevolutionTimeUs = durationUs;
    } else {
        // Establish the first reference point
        isFirstPulse = false;
    }

    // 4. Update the timestamp for the next revolution calculation
    lastZPulseCaptureUs = currentCaptureUs;
    SEGGER_RTT_WriteString(0, "Encoder Z-Pulse Detected!\n");
    SEGGER_RTT_printf(0, "Cycle Time: %u us \n", lastRevolutionTimeUs );

    DEBUG_PrintZPulseTiming();
}



void App_Encoder_Init() 
{
   
    HAL_Encoder_Init();
    // Subscribe to Z-phase! Pass the address of our function.
    HAL_Encoder_Init_ZeroInput(App_Encoder_OnZeroPulse);
    HAL_Encoder_TimerHires_Init();
}
/**
 * @brief  Gets the current absolute position of the encoder in pulses. 
 * @note This is a direct read from the hardware counter.
 * 
 * @return uint16_t 
 */
uint16_t App_Encoder_GetPosition() 
{
    return (uint16_t)HAL_Encoder_GetRawCount();

}



/**
 * @brief  Sets the logical counting direction of the encoder dynamically.
 *         This is useful for aligning software direction with mechanical setup.
 * @param  reversed: If true, reverses the counting direction.
 */
void App_Encoder_SetDirection(bool reversed) {
    // Pass the state down to the Hardware Abstraction Layer.
    // Note: We invert the boolean here because your HAL expects 'true' for Normal 
    // and 'false' for Reversed logic based on your current HAL implementation.
    HAL_Encoder_SetCountDirection(!reversed);
    
    // Output diagnostic message to the debug terminal
    if (reversed) {
        SEGGER_RTT_WriteString(0, "Encoder Direction changed to: REVERSED\n");
    } else {
        SEGGER_RTT_WriteString(0, "Encoder Direction changed to: NORMAL\n");
    }
}

void App_Encoder_ResetCycleTime(void) 
{ 
}

