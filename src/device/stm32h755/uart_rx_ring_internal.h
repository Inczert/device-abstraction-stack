// SPDX-License-Identifier: Apache-2.0
#ifndef DAS_STM32H755_UART_RX_RING_INTERNAL_H
#define DAS_STM32H755_UART_RX_RING_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

/*
 * One interrupt producer and one task consumer on the SAME Cortex-M core.
 * uint32_t index loads/stores are naturally aligned and atomic on CM7/CM4.
 * Acquire/release ordering for storage is provided by the Cortex-M DMB in
 * the production driver via DAS_UART_RX_RING_BARRIER().
 */
#ifndef DAS_UART_RX_RING_BARRIER
#define DAS_UART_RX_RING_BARRIER() ((void)0)
#endif

typedef struct das_uart_rx_ring {
    uint8_t* storage;
    uint32_t capacity;
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t stored;
    volatile uint32_t dropped;
    volatile uint32_t errors;
} das_uart_rx_ring_t;

static inline void das_uart_rx_ring_reset(das_uart_rx_ring_t* ring,
                                          uint8_t* storage,
                                          uint32_t capacity) {
    ring->storage = storage;
    ring->capacity = capacity;
    ring->head = 0u;
    ring->tail = 0u;
    ring->stored = 0u;
    ring->dropped = 0u;
    ring->errors = 0u;
}

static inline uint32_t das_uart_rx_ring_available(const das_uart_rx_ring_t* ring) {
    return ring->head - ring->tail;
}

static inline bool das_uart_rx_ring_push(das_uart_rx_ring_t* ring, uint8_t value) {
    const uint32_t head = ring->head;
    if ((head - ring->tail) >= ring->capacity) {
        ++ring->dropped;
        return false;
    }
    ring->storage[head % ring->capacity] = value;
    DAS_UART_RX_RING_BARRIER();
    ring->head = head + 1u;
    ++ring->stored;
    return true;
}

static inline bool das_uart_rx_ring_pop(das_uart_rx_ring_t* ring, uint8_t* out) {
    const uint32_t tail = ring->tail;
    if (ring->head == tail) {
        return false;
    }
    DAS_UART_RX_RING_BARRIER();
    *out = ring->storage[tail % ring->capacity];
    DAS_UART_RX_RING_BARRIER();
    ring->tail = tail + 1u;
    return true;
}

#endif /* DAS_STM32H755_UART_RX_RING_INTERNAL_H */
