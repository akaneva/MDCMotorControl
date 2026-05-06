#include "hal_telemetry.h"
#include "stm32f4xx_hal.h"
#include <string.h>

// Hardware handle for the UART peripheral (USART6 used as example)
UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_usart6_tx; // ADDED: DMA Handle for TX

// Single-byte buffer used by the hardware to store the incoming byte
static uint8_t rx_byte;

// Pointer to the application-level state machine callback
static UartRxCallback_t active_rx_callback = NULL;

// Persistent background buffer to hold data during non-blocking transmission
#define TX_BUFFER_SIZE 64
static uint8_t tx_buffer[TX_BUFFER_SIZE];

/**
 * @brief Initialize the telemetry hardware peripheral
 * @param baud Baud rate for the UART
 * @param callback Pointer to the application-level receive callback
 */
void HAL_Telemetry_Init(uint32_t baud, UartRxCallback_t callback) {
    
    // 1. Store the callback reference
    active_rx_callback = callback;

    // 2. Enable peripheral clocks for UART, target GPIO port, and DMA
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE(); // ADDED: Enable DMA2 Clock (Required for USART6 TX)

    // 3. Configure the physical GPIO pins (PC6 = TX, PC7 = RX)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6; // Map pins to USART6 peripheral
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // 4. Configure the UART hardware logic
    huart6.Instance = USART6;
    huart6.Init.BaudRate = baud;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart6);

    // 5. ADDED: Configure DMA for USART6 TX (DMA2, Stream 6, Channel 5)
    hdma_usart6_tx.Instance = DMA2_Stream6;
    hdma_usart6_tx.Init.Channel = DMA_CHANNEL_5;
    hdma_usart6_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart6_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart6_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart6_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart6_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart6_tx.Init.Mode = DMA_NORMAL;
    hdma_usart6_tx.Init.Priority = DMA_PRIORITY_VERY_HIGH; // Highest priority for telemetry
    hdma_usart6_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_usart6_tx);
    
    // Link the DMA handle to the UART handle
    __HAL_LINKDMA(&huart6, hdmatx, hdma_usart6_tx); 

    // 6. Enable NVIC interrupts for UART (RX) and DMA (TX)
    HAL_NVIC_SetPriority(USART6_IRQn, 2, 0); 
    HAL_NVIC_EnableIRQ(USART6_IRQn);
    
    // ADDED: Enable DMA Interrupt
    HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 1, 0); 
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

    // 7. Arm the hardware receiver for the very first byte
    HAL_UART_Receive_IT(&huart6, &rx_byte, 1);
}
/**
 * @brief Set a new baud rate for the telemetry UART. This function will gracefully stop the current UART,
 *        reconfigure it with the new baud rate, and restart it.
 * @param newBaudRate 
 * @return * void 
 */
void HAL_Telemetry_SetBaudRate(uint32_t newBaudRate)
{
    // 1. CRITICAL SECTION START: Disable UART interrupts at the NVIC level.
    HAL_NVIC_DisableIRQ(USART6_IRQn);

    // 2. Abort any active DMA or Interrupt-based transfers to prevent Bus Faults.
    if (huart6.gState != HAL_UART_STATE_READY || huart6.RxState != HAL_UART_STATE_READY) {
        HAL_UART_Abort(&huart6);
    }

    // 3. Disable the UART peripheral (UE bit) to unlock the Baud Rate Register (BRR).
    __HAL_UART_DISABLE(&huart6);

    // 4. Update the baud rate value in the HAL handle structure.
    huart6.Init.BaudRate = newBaudRate;

    // 5. Calculate and write the new value directly to the BRR hardware register.
    uint32_t pclk2_freq = HAL_RCC_GetPCLK2Freq();
    huart6.Instance->BRR = UART_BRR_SAMPLING16(pclk2_freq, newBaudRate);

    // 6. Re-enable the UART peripheral (UE bit).
    __HAL_UART_ENABLE(&huart6);

    // 7. Re-arm the interrupt-driven receiver to listen for incoming bytes.
    HAL_UART_Receive_IT(&huart6, &rx_byte, 1);

    // 8. CRITICAL SECTION END: Re-enable UART interrupts in the NVIC.
    HAL_NVIC_EnableIRQ(USART6_IRQn);
}
/**
 * @brief Write a buffer of data to the telemetry UART
 * @param buffer Pointer to the data buffer to be sent
 * @param length Length of the data buffer in bytes
 * @return true if the transmission was successfully initiated, false otherwise
 */
bool HAL_Telemetry_WriteBuffer(const uint8_t *buffer, size_t length) {

     
    // Reject data if it exceeds our static memory allocation
    if (length > TX_BUFFER_SIZE) {
        return false; 
    }
    // Concurrency protection: Do not overwrite the buffer if a transmission is ongoing
    if (huart6.gState != HAL_UART_STATE_READY) {
        return false; 
    }

    // Safely copy the application data to the persistent hardware transmission buffer
    memcpy(tx_buffer, buffer, length);

    // CHANGED: Instruct the DMA controller to start sending data in the background.
    // This is 100% non-blocking. The CPU returns immediately.
    if (HAL_UART_Transmit_DMA(&huart6, tx_buffer, length) == HAL_OK) {
        return true;
    }
    else
        return false;
     
}

// ==============================================================================
// HARDWARE INTERRUPT SERVICE ROUTINES (ISR)
// ==============================================================================
// ADDED: Hardware ISR for DMA Stream 6
void DMA2_Stream6_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_usart6_tx);
}

void USART6_IRQHandler(void) {
    
    // The HAL_UART_IRQHandler acts as a universal dispatcher.
    // It reads the hardware status registers (SR), determines the exact cause 
    // of the interrupt (RX, TX, or Error), clears the pending hardware flags, 
    // and seamlessly routes the execution to the appropriate weak callback.
    HAL_UART_IRQHandler(&huart6);
}

// This callback is executed by HAL_UART_IRQHandler ONLY if the interrupt 
// was caused by a successful byte reception (RXNE flag set).

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) {
        
        // Push the byte upwards to the application's protocol parser
        if (active_rx_callback != NULL) {
            active_rx_callback(rx_byte);
        }
        
        // Rearm the hardware trap to listen for the next incoming byte
        HAL_UART_Receive_IT(&huart6, &rx_byte, 1);
    }
}

/* 
 * CRITICAL: 
 * When ORE (Overrun Error) occurs, the HAL library calls this function.
 * Without it, the UART peripheral stays in an error state and stops responding.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) {
       
        /* 1. Abort the ongoing reception to unlock the HAL state machine */
        HAL_UART_AbortReceive(huart);
        
                
        __HAL_UART_CLEAR_OREFLAG(huart);    // Clear ORE flag (Must be cleared first to exit the error state)
        __HAL_UART_CLEAR_FEFLAG(huart);     // Clear FE flag (Framing Error)
        __HAL_UART_CLEAR_NEFLAG(huart);     // Clear NE flag (Noise Error)
        __HAL_UART_CLEAR_PEFLAG(huart);     // Clear PE flag (Parity Error)
        __HAL_UART_CLEAR_IDLEFLAG(huart);   // Clear IDLE flag (Idle Line Detected)

        // Restart the interrupt-driven reception, otherwise it remains disabled
        HAL_UART_Receive_IT(huart, &rx_byte, 1);
    }
}

// Callback triggered automatically by the HAL when the entire TX buffer has been dispatched

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) {
        // Transmission via DMA is 100% complete.
        // Optional: Toggle RS485 DE/RE pin if used.
    }
}
