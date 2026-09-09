// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_UART_H
#define DAS_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <das/result.h>

/** Opaque UART resource handle. storage is backend-owned. */
typedef struct das_uart {
    uint32_t storage;
} das_uart_t;

#define DAS_UART_INVALID ((das_uart_t){UINT32_MAX})
#define DAS_UART_WAIT_FOREVER UINT32_MAX

/** Number of application data bits, excluding any parity bit. */
typedef enum das_uart_data_bits {
    DAS_UART_DATA_BITS_7 = 7,
    DAS_UART_DATA_BITS_8 = 8
} das_uart_data_bits_t;

typedef enum das_uart_parity {
    DAS_UART_PARITY_NONE = 0,
    DAS_UART_PARITY_EVEN,
    DAS_UART_PARITY_ODD
} das_uart_parity_t;

typedef enum das_uart_stop_bits {
    DAS_UART_STOP_BITS_1 = 1,
    DAS_UART_STOP_BITS_2 = 2
} das_uart_stop_bits_t;

typedef struct das_uart_config {
    uint32_t baud_rate;
    das_uart_data_bits_t data_bits;
    das_uart_parity_t parity;
    das_uart_stop_bits_t stop_bits;
} das_uart_config_t;

/** Return true when the selected backend recognizes this UART handle. */
bool das_uart_is_valid(das_uart_t uart);

/** Configure and enable a UART whose board/device route has already been selected. */
das_result_t das_uart_init(das_uart_t uart, const das_uart_config_t* config);

/** Return the effective baud rate derived from live peripheral clocking and BRR. */
das_result_t das_uart_get_baud_rate(das_uart_t uart, uint32_t* baud_rate);

/** Blocking transmit. Waits indefinitely for hardware readiness. */
das_result_t das_uart_write(das_uart_t uart, const uint8_t* data, size_t size);

/** Blocking receive. Waits indefinitely for incoming bytes. */
das_result_t das_uart_read(das_uart_t uart, uint8_t* data, size_t size);

/**
 * Transmit with an operation-wide timeout in milliseconds.
 *
 * A finite timeout requires an active DAS monotonic time source. Use
 * DAS_UART_WAIT_FOREVER to wait without requiring das_time_init().
 */
das_result_t das_uart_write_timeout(das_uart_t uart,
                                    const uint8_t* data,
                                    size_t size,
                                    uint32_t timeout_ms);

/**
 * Receive with an operation-wide timeout in milliseconds.
 *
 * A finite timeout requires an active DAS monotonic time source. Receive
 * framing/parity/noise/overrun faults return DAS_ERROR_IO.
 */
das_result_t das_uart_read_timeout(das_uart_t uart,
                                   uint8_t* data,
                                   size_t size,
                                   uint32_t timeout_ms);

/** Wait until the transmitter has completely shifted out the final frame. */
das_result_t das_uart_flush(das_uart_t uart);

#endif
