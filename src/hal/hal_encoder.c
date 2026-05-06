/**
 * @file hal_encoder.c
 * @author restartevskiy@gmail.com
 * @brief 
 * @version 0.1
 * @date 2026-05-07
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "hal_encoder.h"
#include "stm32f4xx_hal.h"


// ENCODER PROPERTIES & CONFIGURATION
#define ENCODER_PPR             3600                        // Pulses Per Revolution
#define ENCODER_X4_RESOLUTION   (ENCODER_PPR * 4)           // 4x mode resolution (14400)
#define ENCODER_MAX_COUNT       ((ENCODER_X4_RESOLUTION)  - 1 ) // Timer rollover (14399)

// --- Hardware Pinout Definitions ---
#define ENCODER_TIMER           TIM4

// A and B Phase Pins (PB6 and PB7 for TIM4)
#define ENCODER_AB_PORT         GPIOB
#define ENCODER_OUT_A_PIN       GPIO_PIN_6   
#define ENCODER_OUT_B_PIN       GPIO_PIN_7   
                  

// Z-Pulse Pin (PE2 corresponds to EXTI Line 2)
#define ENCODER_ZERO_PORT         GPIOE
#define ENCODER_ZERO_PIN          GPIO_PIN_2

// Define timer control structure according to STM32 HAL
static TIM_HandleTypeDef htim4;

static TIM_HandleTypeDef htim5;  // 

static EncoderZeroCallback_t activeZeroCallback = NULL; // Global pointer to store the application-level callback for Z-pulse (Zero) events
static EncoderStepCallback_t activeStepCallback = NULL; // Global pointer to store the application-level callback

static uint16_t lastSentPos = 0xFFFF; // Software Filter to prevent noise triggering

uint32_t HAL_Encoder_GetHiresTimeUs(void) {
    // Direct register access: reads the hardware counter in a single CPU cycle
    return TIM5->CNT; 
}

/**
 * @brief Initializes TIM5 as a free-running 32-bit hardware stopwatch.
 *        Operating at 1 MHz, it provides microsecond precision for time measurements.
 *        A 32-bit timer at 1 MHz rolls over approximately every 71.5 minutes, 
 *        eliminating the need for overflow handling in our fast cycle times.
 */
void HAL_Encoder_TimerHires_Init(void) {
    // 1. Enable the peripheral clock for TIM5
    __HAL_RCC_TIM5_CLK_ENABLE();

    // 2. Configure TIM5 to tick at exactly 1 MHz (1 tick = 1 microsecond)
    // The APB1 timer clock on STM32F407 (with 168MHz sysclk) is typically 84 MHz.
    htim5.Instance = TIM5;
    htim5.Init.Prescaler = 84 - 1;                // 84MHz / 84 = 1 MHz
    htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim5.Init.Period = 0xFFFFFFFF;               // Maximum 32-bit value (~4.29 billion)
    htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_Base_Init(&htim5);

    // 3. Start the timer immediately in free-running mode
    HAL_TIM_Base_Start(&htim5);
}

void HAL_Encoder_Init() {
    // 1. Enable clock signal to Timer 4, Port B 
    __HAL_RCC_TIM4_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // 2. Configure pins PB6 and PB7 for Alternate Function (AF2 = TIM4)

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = ENCODER_OUT_A_PIN | ENCODER_OUT_B_PIN; 
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;  // For Totem Pole or Line Driver, NOPULL is ideal.
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM4; 
    HAL_GPIO_Init(ENCODER_AB_PORT, &GPIO_InitStruct);

    // =========================================================================
    // NEW: CONFIGURE EXTI ON PB6 AND PB7 (For absolute maximum resolution)
    // =========================================================================
    __HAL_RCC_SYSCFG_CLK_ENABLE(); // Enable SYSCFG clock for EXTI routing
    
    // Route EXTI line 6 and 7 to Port B (Bits 8-15 in EXTICR[1])

    //  Clear the configuration bits for EXTI line 6 and 7  
    SYSCFG->EXTICR[1] &= ~(SYSCFG_EXTICR2_EXTI6 | SYSCFG_EXTICR2_EXTI7);
    // Route EXTI line 6 and 7 specifically to Port B
    SYSCFG->EXTICR[1] |= (SYSCFG_EXTICR2_EXTI6_PB | SYSCFG_EXTICR2_EXTI7_PB);
    
    // Enable NVIC priority for EXTI lines 5 through 9
    // Priority 1 ensures we never miss a physical edge from the encoder
    
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 1, 0); 
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    // 3. Configure Timer 4 itself

    htim4.Instance = TIM4;
    htim4.Init.Prescaler = 0; // Want maximum resolution (divisor 0)
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;

    htim4.Init.Period = ENCODER_MAX_COUNT; // Set the period to max count (14399) for proper rollover
    htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    // 4. CONFIGURE ENCODER MODE (Quadrature)
    TIM_Encoder_InitTypeDef sConfig = {0};
    sConfig.EncoderMode = TIM_ENCODERMODE_TI12; // TIM_ENCODERMODE_TI12 means X4 mode (count edges on both phases)

    // Settings for Phase A (Channel 1)
    sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;        // do not invert polarity for A, as it is used for counting pulses
    sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;   
    sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC1Filter = 15;                             // max filter value to handle noisy signals, adjust as needed

    // Settings for Phase B (Channel 2)
    sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;   // do not invert polarity for B, as it is used for direction sensing.
    sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC2Filter = 15;                         // max filter value to handle noisy signals, adjust as needed

    // 5. Initialize and start hardware!
    HAL_TIM_Encoder_Init(&htim4, &sConfig);
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
}

void HAL_Encoder_Init_ZeroInput(EncoderZeroCallback_t callback) {
    if (callback == NULL) return;
    
    activeZeroCallback = callback;

    // 1. Enable Clock for Port E
    __HAL_RCC_GPIOE_CLK_ENABLE();

    // 2. Configure PE2 as External Interrupt (EXTI) with Rising Edge trigger
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = ENCODER_ZERO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;     // RISING edge matches the typical logic for an active-high Z-pulse
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;             // Assuming Open-Collector/NPN. Change to NOPULL if Push-Pull.
    

    HAL_GPIO_Init(ENCODER_ZERO_PORT, &GPIO_InitStruct);

    // 3. Configure NVIC Priority and Enable the Interrupt
    // Priority 2: Slightly lower than stepper motor (Priority 1) to prevent missing motor steps.
    HAL_NVIC_SetPriority(EXTI2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);
}

// Register the application-level callback for step events
void HAL_Encoder_Init_StepCallback(EncoderStepCallback_t callback) {
    activeStepCallback = callback;
}

void HAL_Encoder_SetCountDirection(bool direction) {
    
    if (direction) {
        // Normal direction: Non-inverted polarity on Channel 1
        // Clear the CC1P (Capture/Compare 1 output Polarity) bit
        TIM4->CCER &= ~TIM_CCER_CC1P;
    } else {
        // Reversed direction: Inverted polarity on Channel 1
        // Set the CC1P bit to treat falling edges as the active state
        TIM4->CCER |= TIM_CCER_CC1P;
    }
}

int32_t HAL_Encoder_GetRawCount() {
    // Directly read processor hardware counter for 1 clock tick!
    return (int32_t)(__HAL_TIM_GET_COUNTER(&htim4));
}

void HAL_Encoder_EnableInterrupts(void) {
    // Unmask EXTI 6 and 7 (Allow interrupts to pass through)
    EXTI->IMR |= (EXTI_IMR_MR6 | EXTI_IMR_MR7);
    
    // Trigger on BOTH Rising and Falling edges for 4x resolution!
    EXTI->RTSR |= (EXTI_IMR_MR6 | EXTI_IMR_MR7);
    EXTI->FTSR |= (EXTI_IMR_MR6 | EXTI_IMR_MR7);
}

void HAL_Encoder_DisableInterrupts(void) {
    // Mask EXTI 6 and 7 (Stop interrupts)
    EXTI->IMR &= ~(EXTI_IMR_MR6 | EXTI_IMR_MR7);
}

void HAL_Encoder_Reset() {
    // Write 0 directly to the register
    __HAL_TIM_SET_COUNTER(&htim4, 0);
}

// ==============================================================================
// HARDWARE INTERRUPT SERVICE ROUTINE (EXTI)
// ==============================================================================
/**
 * @brief Interrupt Service Routine for the Encoder Step input. This function is triggered by a rising or falling edge on PE6 or PE7, which corresponds to EXTI Line 6 and 7.
 * 
 */

void EXTI9_5_IRQHandler(void) {
    
    // Check if the interrupt was caused by PB6 or PB7 using HAL macro
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_6 | GPIO_PIN_7) != RESET) {
        
        // 1. Clear the hardware pending flags immediately using HAL macro
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_6 | GPIO_PIN_7);
        
        // 2. Check if application is subscribed
        if (activeStepCallback != NULL) {
            
            // Grab the count using the official HAL macro
            uint16_t currentPos = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
            
            // Software Filter to prevent noise triggering
            
            
            if (currentPos != lastSentPos) {
                lastSentPos = currentPos;
                activeStepCallback(currentPos);
            }
        }
    }
}

/**
 * @brief Interrupt Service Routine for the Encoder Z-Pulse (Zero) input. This function is triggered by a rising edge on PE2, which corresponds to EXTI Line 2.
 * 
 */
void EXTI2_IRQHandler(void) {

    // 1. Check if the interrupt was triggered specifically by Pin 2
    if (__HAL_GPIO_EXTI_GET_IT(ENCODER_ZERO_PIN) != RESET) {
        
        // 2. Clear the hardware pending flag immediately!
        // If not cleared, the CPU will get stuck in this ISR forever.
        __HAL_GPIO_EXTI_CLEAR_IT(ENCODER_ZERO_PIN);
        
        // 3. Execute the upper-layer callback (Resetting the logic count)
        if (activeZeroCallback != NULL) {
            activeZeroCallback();
        }
    }
}

