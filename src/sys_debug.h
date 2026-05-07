#ifndef SYS_DEBUG_H
#define SYS_DEBUG_H

// If the flag is not provided by the compiler (e.g., via platformio.ini), 
// default to 0 (disabled / Olimex mode).
#ifndef USE_SEGGER_RTT
    #define USE_SEGGER_RTT 0
#endif

#if USE_SEGGER_RTT
    // Include the actual SEGGER RTT library only if enabled (J-Link mode).
    #include "SEGGER_RTT.h"
#else
    // Replace SEGGER RTT functions with empty macros (Dummy functions).
    // The compiler will ignore these calls, preventing crashes and saving memory.
    #define SEGGER_RTT_Init()                      ((void)0)
    #define SEGGER_RTT_WriteString(BufferIndex, s) ((void)0)
    #define SEGGER_RTT_printf(BufferIndex, ...)    ((void)0)
#endif

#endif // SYS_DEBUG_H