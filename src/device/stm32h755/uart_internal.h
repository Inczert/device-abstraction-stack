// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_STM32H755_UART_INTERNAL_H
#define DAS_STM32H755_UART_INTERNAL_H

#include <das/uart.h>

typedef enum stm32h755_uart_instance {
    STM32H755_UART_USART1 = 1,
    STM32H755_UART_USART3 = 3
} stm32h755_uart_instance_t;

/** Construct a generic DAS handle for a supported STM32H755 UART instance. */
das_uart_t stm32h755_uart_handle(stm32h755_uart_instance_t instance);

#endif
