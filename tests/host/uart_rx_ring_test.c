// SPDX-License-Identifier: Apache-2.0

/* Host contract for the ISR-producer/task-consumer UART ring. */
#include "uart_rx_ring_internal.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    uint8_t storage[4] = {0u};
    das_uart_rx_ring_t ring = {0};
    das_uart_rx_ring_reset(&ring, storage, 4u);
    assert(das_uart_rx_ring_available(&ring) == 0u);
    for (uint32_t i = 0u; i < 4u; ++i) {
        assert(das_uart_rx_ring_push(&ring, (uint8_t)(i + 1u)));
    }
    assert(das_uart_rx_ring_available(&ring) == 4u);
    assert(!das_uart_rx_ring_push(&ring, UINT8_C(255)));
    assert(ring.dropped == 1u);
    uint8_t value = 0u;
    assert(das_uart_rx_ring_pop(&ring, &value) && value == 1u);
    assert(das_uart_rx_ring_pop(&ring, &value) && value == 2u);
    assert(das_uart_rx_ring_push(&ring, UINT8_C(5)));
    assert(das_uart_rx_ring_push(&ring, UINT8_C(6)));
    assert(das_uart_rx_ring_available(&ring) == 4u);
    for (uint8_t expected = 3u; expected <= 6u; ++expected) {
        assert(das_uart_rx_ring_pop(&ring, &value) && value == expected);
    }
    assert(!das_uart_rx_ring_pop(&ring, &value));
    assert(ring.stored == 6u);
    assert(ring.dropped == 1u);

    /* Index rollover retains correct occupancy and ordering. */
    das_uart_rx_ring_reset(&ring, storage, 4u);
    ring.head = UINT32_MAX - 1u;
    ring.tail = UINT32_MAX - 1u;
    for (uint8_t i = 10u; i < 14u; ++i) {
        assert(das_uart_rx_ring_push(&ring, i));
    }
    assert(das_uart_rx_ring_available(&ring) == 4u);
    for (uint8_t i = 10u; i < 14u; ++i) {
        assert(das_uart_rx_ring_pop(&ring, &value) && value == i);
    }
    assert(das_uart_rx_ring_available(&ring) == 0u);
    puts("UART ring regression: PASS");
    return 0;
}
