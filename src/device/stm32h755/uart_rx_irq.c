// SPDX-License-Identifier: Apache-2.0
/*
 * Optional interrupt-buffered STM32H755 UART RX.
 *
 * Deliberately separate from uart.c: libdas.a only extracts this object
 * when firmware calls the buffered RX API. The canonical weak vector-table
 * handlers call its linked service hooks, preserving application IRQ handler
 * overrides on unrelated or polling-only USART instances.
 */
#include <das/uart.h>

#include "stm32h755xx.h"
#include "uart_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DAS_UART_RX_RING_BARRIER() __DMB()
#include "uart_rx_ring_internal.h"

#define DAS_UART_RX_ISR_MAX_BYTES 32u
#define DAS_UART_RX_MAX_CAPACITY UINT32_C(65535)
#define DAS_UART_RX_ERROR_FLAGS (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)
#define DAS_UART_RX_ERROR_CLEAR (USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_ORECF)

typedef struct das_uart_rx_context {
    das_uart_rx_ring_t ring;
    bool active;
} das_uart_rx_context_t;

static das_uart_rx_context_t g_rx_usart1;
static das_uart_rx_context_t g_rx_usart3;

static das_result_t rx_resolve(das_uart_t uart,
                                USART_TypeDef** registers,
                                IRQn_Type* irq,
                                das_uart_rx_context_t** ctx) {
    if (registers == 0 || irq == 0 || ctx == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (uart.storage == (uint32_t)STM32H755_UART_USART1) {
        *registers = USART1;
        *irq = USART1_IRQn;
        *ctx = &g_rx_usart1;
        return DAS_OK;
    }
    if (uart.storage == (uint32_t)STM32H755_UART_USART3) {
        *registers = USART3;
        *irq = USART3_IRQn;
        *ctx = &g_rx_usart3;
        return DAS_OK;
    }
    return DAS_ERROR_INVALID_ARGUMENT;
}

static uint32_t rx_mask(const USART_TypeDef* registers) {
    const uint32_t cr1 = registers->CR1;
    uint32_t bits = (cr1 & USART_CR1_M1) != 0u ? 7u :
                    (cr1 & USART_CR1_M0) != 0u ? 9u : 8u;
    if ((cr1 & USART_CR1_PCE) != 0u) {
        --bits;
    }
    return bits >= 8u ? UINT32_C(0xff) : (UINT32_C(1) << bits) - 1u;
}

static void uart_rx_service(USART_TypeDef* registers,
                            das_uart_rx_context_t* ctx) {
    if (!ctx->active) {
        return;
    }

    const uint32_t mask = rx_mask(registers);
    /* Bound the time spent in the ISR even under continuous traffic. */
    for (uint32_t iteration = 0u; iteration < DAS_UART_RX_ISR_MAX_BYTES; ++iteration) {
        const uint32_t status = registers->ISR;
        if ((status & DAS_UART_RX_ERROR_FLAGS) != 0u) {
            ++ctx->ring.errors;
            registers->ICR = DAS_UART_RX_ERROR_CLEAR;
            if ((status & USART_ISR_RXNE_RXFNE) != 0u) {
                registers->RQR = USART_RQR_RXFRQ;
            }
            continue;
        }
        if ((status & USART_ISR_RXNE_RXFNE) == 0u) {
            break;
        }
        const uint8_t byte = (uint8_t)(registers->RDR & mask);
        (void)das_uart_rx_ring_push(&ctx->ring, byte);
    }
}

void das_stm32h755_uart1_rx_service(void) {
    uart_rx_service(USART1, &g_rx_usart1);
}

void das_stm32h755_uart3_rx_service(void) {
    uart_rx_service(USART3, &g_rx_usart3);
}

das_result_t das_uart_rx_buffer_start(das_uart_t uart,
                                      uint8_t* storage,
                                      size_t capacity) {
    USART_TypeDef* registers;
    IRQn_Type irq;
    das_uart_rx_context_t* ctx;
    if (rx_resolve(uart, &registers, &irq, &ctx) != DAS_OK ||
        storage == 0 || capacity < 2u || capacity > DAS_UART_RX_MAX_CAPACITY) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if ((registers->CR1 & (USART_CR1_UE | USART_CR1_RE)) !=
         (USART_CR1_UE | USART_CR1_RE)) {
        return DAS_ERROR_NOT_READY;
    }

    const uint32_t old_primask = __get_PRIMASK();
    __disable_irq();
    if (ctx->active) {
        __set_PRIMASK(old_primask);
        return DAS_ERROR_NOT_READY;
    }
    NVIC_DisableIRQ(irq);
    registers->CR1 &= ~(USART_CR1_RXNEIE_RXFNEIE | USART_CR1_PEIE);
    registers->CR3 &= ~USART_CR3_EIE;
    registers->ICR = DAS_UART_RX_ERROR_CLEAR;
    registers->RQR = USART_RQR_RXFRQ;
    das_uart_rx_ring_reset(&ctx->ring, storage, (uint32_t)capacity);
    __DMB();
    ctx->active = true;
    NVIC_ClearPendingIRQ(irq);
    registers->CR3 |= USART_CR3_EIE;
    registers->CR1 |= USART_CR1_RXNEIE_RXFNEIE | USART_CR1_PEIE;
    NVIC_EnableIRQ(irq);
    __set_PRIMASK(old_primask);
    return DAS_OK;
}

das_result_t das_uart_rx_buffer_stop(das_uart_t uart) {
    USART_TypeDef* registers;
    IRQn_Type irq;
    das_uart_rx_context_t* ctx;
    if (rx_resolve(uart, &registers, &irq, &ctx) != DAS_OK) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    const uint32_t old_primask = __get_PRIMASK();
    __disable_irq();
    if (!ctx->active) {
        __set_PRIMASK(old_primask);
        return DAS_ERROR_NOT_READY;
    }
    registers->CR1 &= ~(USART_CR1_RXNEIE_RXFNEIE | USART_CR1_PEIE);
    registers->CR3 &= ~USART_CR3_EIE;
    NVIC_DisableIRQ(irq);
    NVIC_ClearPendingIRQ(irq);
    ctx->active = false;
    __DMB();
    ctx->ring.storage = 0;
    ctx->ring.capacity = 0u;
    __set_PRIMASK(old_primask);
    return DAS_OK;
}

das_result_t das_uart_rx_buffer_read(das_uart_t uart,
                                     uint8_t* data,
                                     size_t size,
                                     size_t* received) {
    USART_TypeDef* registers;
    IRQn_Type irq;
    das_uart_rx_context_t* ctx;
    if (rx_resolve(uart, &registers, &irq, &ctx) != DAS_OK ||
        received == 0 || (size != 0u && data == 0)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    (void)registers;
    (void)irq;
    *received = 0u;
    if (!ctx->active) {
        return DAS_ERROR_NOT_READY;
    }
    for (; *received < size; ++*received) {
        if (!das_uart_rx_ring_pop(&ctx->ring, &data[*received])) {
            break;
        }
    }
    return DAS_OK;
}

das_result_t das_uart_rx_buffer_stats(das_uart_t uart,
                                      das_uart_rx_stats_t* stats) {
    USART_TypeDef* registers;
    IRQn_Type irq;
    das_uart_rx_context_t* ctx;
    if (stats == 0 || rx_resolve(uart, &registers, &irq, &ctx) != DAS_OK) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    (void)registers;
    (void)irq;
    const uint32_t old_primask = __get_PRIMASK();
    __disable_irq();
    if (!ctx->active) {
        __set_PRIMASK(old_primask);
        return DAS_ERROR_NOT_READY;
    }
    stats->bytes_stored = ctx->ring.stored;
    stats->bytes_dropped = ctx->ring.dropped;
    stats->hardware_errors = ctx->ring.errors;
    stats->bytes_available = (size_t)das_uart_rx_ring_available(&ctx->ring);
    stats->capacity = (size_t)ctx->ring.capacity;
    __set_PRIMASK(old_primask);
    return DAS_OK;
}
