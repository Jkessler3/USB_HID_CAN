#ifndef JOYSTICK_UART_TRACE_H
#define JOYSTICK_UART_TRACE_H

#include <stdbool.h>
#include <stdio.h>
#include "stm32f4xx.h"

/* Override on the compiler command line for a dedicated bench build. */
#ifndef JOYSTICK_UART_TRACE
#ifdef DEBUG
#define JOYSTICK_UART_TRACE 1
#else
#define JOYSTICK_UART_TRACE 0
#endif
#endif

#if JOYSTICK_UART_TRACE != 0 && JOYSTICK_UART_TRACE != 1
#error "JOYSTICK_UART_TRACE must be 0 or 1"
#endif

static inline bool joystick_uart_trace_enabled(void)
{
    return JOYSTICK_UART_TRACE && (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) == 0U;
}

#define JOYSTICK_TRACE_PRINTF(...) do {             \
    if (joystick_uart_trace_enabled()) {            \
        printf(__VA_ARGS__);                        \
    }                                               \
} while (0)

#endif
