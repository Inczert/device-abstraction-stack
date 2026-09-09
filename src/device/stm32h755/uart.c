// SPDX-License-Identifier: Apache-2.0

#include <das/time.h>
#include <das/uart.h>

#include "clock_internal.h"
#include "stm32h755xx.h"
#include "uart_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DAS_STM32H755_UART_WAIT_LIMIT UINT32_C(1000000)
#define DAS_STM32H755_UART_BRR_MIN UINT32_C(0x10)
#define DAS_STM32H755_UART_BRR_MAX UINT32_C(0xffff)
#define DAS_STM32H755_UART_ERROR_FLAGS \
    (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)
#define DAS_STM32H755_UART_CLEAR_ERRORS \
    (USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_ORECF)

#if defined(CORE_CM7)
#define DAS_RCC_CORE RCC_C1
#elif defined(CORE_CM4)
#define DAS_RCC_CORE RCC_C2
#else
#error "STM32H755 UART backend requires CORE_CM7 or CORE_CM4"
#endif

typedef struct stm32h755_uart_route {
    USART_TypeDef* registers;
    bool apb2;
} stm32h755_uart_route_t;

typedef struct uart_wait {
    bool finite;
    das_time_ms_t start_ms;
    uint32_t timeout_ms;
} uart_wait_t;

static bool config_valid(const das_uart_config_t* config) {
    if (config == 0 || config->baud_rate == 0u) {
        return false;
    }

    if (config->data_bits != DAS_UART_DATA_BITS_7 &&
        config->data_bits != DAS_UART_DATA_BITS_8) {
        return false;
    }

    if (config->parity != DAS_UART_PARITY_NONE &&
        config->parity != DAS_UART_PARITY_EVEN &&
        config->parity != DAS_UART_PARITY_ODD) {
        return false;
    }

    return config->stop_bits == DAS_UART_STOP_BITS_1 ||
           config->stop_bits == DAS_UART_STOP_BITS_2;
}

static das_result_t resolve_uart(das_uart_t uart, stm32h755_uart_route_t* route) {
    if (route == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    switch (uart.storage) {
        case STM32H755_UART_USART1:
            route->registers = USART1;
            route->apb2 = true;
            return DAS_OK;
        case STM32H755_UART_USART3:
            route->registers = USART3;
            route->apb2 = false;
            return DAS_OK;
        default:
            return DAS_ERROR_INVALID_ARGUMENT;
    }
}

static void select_pclk_source(das_uart_t uart) {
    if (uart.storage == STM32H755_UART_USART1) {
#if defined(RCC_D2CCIP2R_USART16SEL)
        RCC->D2CCIP2R &= ~RCC_D2CCIP2R_USART16SEL;
#else
#error "STM32H755 CMSIS header does not expose USART1/6 kernel-clock selection"
#endif
    } else {
#if defined(RCC_D2CCIP2R_USART28SEL)
        RCC->D2CCIP2R &= ~RCC_D2CCIP2R_USART28SEL;
#elif defined(RCC_D2CCIP2R_USART234578SEL)
        RCC->D2CCIP2R &= ~RCC_D2CCIP2R_USART234578SEL;
#else
#error "STM32H755 CMSIS header does not expose USART2/3/4/5/7/8 kernel-clock selection"
#endif
    }
    __DSB();
}

static void enable_peripheral_clock(das_uart_t uart) {
    if (uart.storage == STM32H755_UART_USART1) {
        DAS_RCC_CORE->APB2ENR |= RCC_APB2ENR_USART1EN;
        (void)DAS_RCC_CORE->APB2ENR;
    } else {
        DAS_RCC_CORE->APB1LENR |= RCC_APB1LENR_USART3EN;
        (void)DAS_RCC_CORE->APB1LENR;
    }
    __DSB();
}

static das_result_t kernel_frequency(das_uart_t uart, uint32_t* frequency_hz) {
    if (frequency_hz == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    stm32h755_uart_route_t route;
    if (resolve_uart(uart, &route) != DAS_OK) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    stm32h755_clock_frequencies_t clocks = {0};
    const das_result_t result = stm32h755_clock_get_frequencies(0u, &clocks);
    if (result != DAS_OK) {
        return result;
    }

    *frequency_hz = route.apb2 ? clocks.apb2_hz : clocks.apb1_hz;
    return *frequency_hz == 0u ? DAS_ERROR_NOT_READY : DAS_OK;
}

static das_result_t prepare_wait(uint32_t timeout_ms, uart_wait_t* wait) {
    if (wait == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    if (timeout_ms == DAS_UART_WAIT_FOREVER) {
        *wait = (uart_wait_t){.finite = false, .start_ms = 0u, .timeout_ms = 0u};
        return DAS_OK;
    }

    if (timeout_ms > DAS_TIME_MAX_INTERVAL_MS) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (!das_time_is_ready()) {
        return DAS_ERROR_NOT_READY;
    }

    *wait = (uart_wait_t){
        .finite = true,
        .start_ms = das_time_now_ms(),
        .timeout_ms = timeout_ms,
    };
    return DAS_OK;
}

static bool wait_expired(const uart_wait_t* wait) {
    return wait->finite &&
           das_time_interval_elapsed(wait->start_ms, wait->timeout_ms);
}

static das_result_t wait_for_flag(USART_TypeDef* registers,
                                  uint32_t flag,
                                  bool check_rx_errors,
                                  const uart_wait_t* wait) {
    for (;;) {
        const uint32_t status = registers->ISR;
        if (check_rx_errors && (status & DAS_STM32H755_UART_ERROR_FLAGS) != 0u) {
            registers->ICR = DAS_STM32H755_UART_CLEAR_ERRORS;
            return DAS_ERROR_IO;
        }
        if ((status & flag) != 0u) {
            return DAS_OK;
        }
        if (wait_expired(wait)) {
            return DAS_ERROR_TIMEOUT;
        }
    }
}

static das_result_t wait_for_enable_ack(USART_TypeDef* registers) {
    const uint32_t required = USART_ISR_TEACK | USART_ISR_REACK;
    for (uint32_t attempt = 0u; attempt < DAS_STM32H755_UART_WAIT_LIMIT; ++attempt) {
        if ((registers->ISR & required) == required) {
            return DAS_OK;
        }
    }
    return DAS_ERROR_TIMEOUT;
}

static uint32_t frame_length_bits(const das_uart_config_t* config) {
    return (uint32_t)config->data_bits +
           (config->parity == DAS_UART_PARITY_NONE ? 0u : 1u);
}

das_uart_t stm32h755_uart_handle(stm32h755_uart_instance_t instance) {
    switch (instance) {
        case STM32H755_UART_USART1:
        case STM32H755_UART_USART3:
            return (das_uart_t){.storage = (uint32_t)instance};
        default:
            return DAS_UART_INVALID;
    }
}

bool das_uart_is_valid(das_uart_t uart) {
    stm32h755_uart_route_t route;
    return resolve_uart(uart, &route) == DAS_OK;
}

das_result_t das_uart_init(das_uart_t uart, const das_uart_config_t* config) {
    stm32h755_uart_route_t route;
    if (resolve_uart(uart, &route) != DAS_OK || !config_valid(config)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    select_pclk_source(uart);
    enable_peripheral_clock(uart);

    uint32_t clock_hz = 0u;
    das_result_t result = kernel_frequency(uart, &clock_hz);
    if (result != DAS_OK) {
        return result;
    }

    const uint32_t brr = (clock_hz + (config->baud_rate / 2u)) / config->baud_rate;
    if (brr < DAS_STM32H755_UART_BRR_MIN || brr > DAS_STM32H755_UART_BRR_MAX) {
        return DAS_ERROR_UNSUPPORTED;
    }

    USART_TypeDef* const registers = route.registers;
    registers->CR1 &= ~USART_CR1_UE;
    registers->CR1 = 0u;
    registers->CR2 = 0u;
    registers->CR3 = 0u;
    registers->PRESC = 0u;

    uint32_t cr1 = USART_CR1_TE | USART_CR1_RE;
    switch (frame_length_bits(config)) {
        case 7u:
            cr1 |= USART_CR1_M1;
            break;
        case 8u:
            break;
        case 9u:
            cr1 |= USART_CR1_M0;
            break;
        default:
            return DAS_ERROR_UNSUPPORTED;
    }

    if (config->parity != DAS_UART_PARITY_NONE) {
        cr1 |= USART_CR1_PCE;
        if (config->parity == DAS_UART_PARITY_ODD) {
            cr1 |= USART_CR1_PS;
        }
    }

    uint32_t cr2 = 0u;
    if (config->stop_bits == DAS_UART_STOP_BITS_2) {
        cr2 |= USART_CR2_STOP_1;
    }

    registers->BRR = brr;
    registers->CR2 = cr2;
    registers->CR3 = 0u;
    registers->ICR = DAS_STM32H755_UART_CLEAR_ERRORS;
    registers->CR1 = cr1 | USART_CR1_UE;
    __DSB();

    result = wait_for_enable_ack(registers);
    if (result != DAS_OK) {
        registers->CR1 &= ~USART_CR1_UE;
    }
    return result;
}

das_result_t das_uart_get_baud_rate(das_uart_t uart, uint32_t* baud_rate) {
    stm32h755_uart_route_t route;
    if (baud_rate == 0 || resolve_uart(uart, &route) != DAS_OK) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if ((route.registers->CR1 & USART_CR1_UE) == 0u || route.registers->BRR == 0u) {
        return DAS_ERROR_NOT_READY;
    }

    uint32_t clock_hz = 0u;
    const das_result_t result = kernel_frequency(uart, &clock_hz);
    if (result != DAS_OK) {
        return result;
    }

    const uint32_t divisor = route.registers->BRR;
    *baud_rate = (clock_hz + (divisor / 2u)) / divisor;
    return DAS_OK;
}

das_result_t das_uart_write_timeout(das_uart_t uart,
                                    const uint8_t* data,
                                    size_t size,
                                    uint32_t timeout_ms) {
    stm32h755_uart_route_t route;
    if ((size != 0u && data == 0) || resolve_uart(uart, &route) != DAS_OK) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (size == 0u) {
        return DAS_OK;
    }
    if ((route.registers->CR1 & USART_CR1_UE) == 0u) {
        return DAS_ERROR_NOT_READY;
    }

    uart_wait_t wait;
    das_result_t result = prepare_wait(timeout_ms, &wait);
    if (result != DAS_OK) {
        return result;
    }

    for (size_t index = 0u; index < size; ++index) {
        result = wait_for_flag(route.registers,
                               USART_ISR_TXE_TXFNF,
                               false,
                               &wait);
        if (result != DAS_OK) {
            return result;
        }
        route.registers->TDR = data[index];
    }

    return wait_for_flag(route.registers, USART_ISR_TC, false, &wait);
}

das_result_t das_uart_read_timeout(das_uart_t uart,
                                   uint8_t* data,
                                   size_t size,
                                   uint32_t timeout_ms) {
    stm32h755_uart_route_t route;
    if ((size != 0u && data == 0) || resolve_uart(uart, &route) != DAS_OK) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (size == 0u) {
        return DAS_OK;
    }
    if ((route.registers->CR1 & USART_CR1_UE) == 0u) {
        return DAS_ERROR_NOT_READY;
    }

    uart_wait_t wait;
    das_result_t result = prepare_wait(timeout_ms, &wait);
    if (result != DAS_OK) {
        return result;
    }

    for (size_t index = 0u; index < size; ++index) {
        result = wait_for_flag(route.registers,
                               USART_ISR_RXNE_RXFNE,
                               true,
                               &wait);
        if (result != DAS_OK) {
            return result;
        }
        data[index] = (uint8_t)(route.registers->RDR & UINT32_C(0xff));
    }
    return DAS_OK;
}

das_result_t das_uart_write(das_uart_t uart, const uint8_t* data, size_t size) {
    return das_uart_write_timeout(uart, data, size, DAS_UART_WAIT_FOREVER);
}

das_result_t das_uart_read(das_uart_t uart, uint8_t* data, size_t size) {
    return das_uart_read_timeout(uart, data, size, DAS_UART_WAIT_FOREVER);
}

das_result_t das_uart_flush(das_uart_t uart) {
    stm32h755_uart_route_t route;
    if (resolve_uart(uart, &route) != DAS_OK) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if ((route.registers->CR1 & USART_CR1_UE) == 0u) {
        return DAS_ERROR_NOT_READY;
    }

    const uart_wait_t wait = {.finite = false, .start_ms = 0u, .timeout_ms = 0u};
    return wait_for_flag(route.registers, USART_ISR_TC, false, &wait);
}
