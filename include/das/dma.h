// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_DMA_H
#define DAS_DMA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <das/irq.h>
#include <das/result.h>

/** Opaque DMA execution resource. */
typedef struct das_dma {
    uint32_t storage;
} das_dma_t;

#define DAS_DMA_INVALID ((das_dma_t){UINT32_MAX})
#define DAS_DMA_WAIT_FOREVER UINT32_MAX

typedef enum das_dma_direction {
    DAS_DMA_PERIPHERAL_TO_MEMORY = 0,
    DAS_DMA_MEMORY_TO_PERIPHERAL,
    DAS_DMA_MEMORY_TO_MEMORY
} das_dma_direction_t;

typedef enum das_dma_width {
    DAS_DMA_WIDTH_BYTE = 1,
    DAS_DMA_WIDTH_HALFWORD = 2,
    DAS_DMA_WIDTH_WORD = 4
} das_dma_width_t;

typedef enum das_dma_state {
    DAS_DMA_STATE_IDLE = 0,
    DAS_DMA_STATE_BUSY,
    DAS_DMA_STATE_COMPLETE,
    DAS_DMA_STATE_ERROR
} das_dma_state_t;

typedef struct das_dma_config {
    das_dma_direction_t direction;
    das_dma_width_t source_width;
    das_dma_width_t destination_width;
    bool source_increment;
    bool destination_increment;
} das_dma_config_t;

/** Acquire one implementation-selected DMA execution resource. */
das_result_t das_dma_acquire(das_dma_t* dma);

/** Abort any active transfer and return a previously acquired resource. */
das_result_t das_dma_release(das_dma_t dma);

/** Return true while this handle refers to a currently acquired resource. */
bool das_dma_is_valid(das_dma_t dma);

/** Configure transfer direction, element widths and address increments. */
das_result_t das_dma_configure(das_dma_t dma, const das_dma_config_t* config);

/**
 * Start a transfer of count elements.
 *
 * source and destination describe the logical data flow irrespective of the
 * target DMA engine's register terminology. The configured element width sets
 * the required address alignment. This call does not perform CPU cache
 * maintenance; use the DAS cache API when DMA-visible memory is cacheable.
 */
das_result_t das_dma_start(das_dma_t dma,
                           const void* source,
                           void* destination,
                           size_t count);

/** Read the current transfer state without waiting. */
das_result_t das_dma_get_state(das_dma_t dma, das_dma_state_t* state);

/** Return the number of transfer elements still pending in hardware. */
das_result_t das_dma_get_remaining(das_dma_t dma, size_t* remaining);

/** Wait indefinitely for completion or a hardware transfer error. */
das_result_t das_dma_wait(das_dma_t dma);

/** Wait for completion using the generic DAS monotonic millisecond source. */
das_result_t das_dma_wait_timeout(das_dma_t dma, uint32_t timeout_ms);

/** Stop a transfer if active and clear its completion/error state. */
das_result_t das_dma_abort(das_dma_t dma);

/** Resolve the generic interrupt line associated with this DMA resource. */
das_result_t das_dma_get_irq(das_dma_t dma, das_irq_t* irq);

#endif
