// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_STM32H755_DMA_HANDLE_INTERNAL_H
#define DAS_STM32H755_DMA_HANDLE_INTERNAL_H

#include <das/dma.h>

#include <stdbool.h>
#include <stdint.h>

/* Low three bits select DMA1 stream 0..7; the upper 29 bits identify the
 * allocation generation. Zero and the all-ones generation are reserved,
 * ensuring no issued handle ever equals DAS_DMA_INVALID (UINT32_MAX). */
#define STM32H755_DMA_HANDLE_INDEX_MASK UINT32_C(7)
#define STM32H755_DMA_HANDLE_GENERATION_LIMIT (UINT32_MAX >> 3u)

static inline uint32_t stm32h755_dma_handle_index(das_dma_t handle) {
    return handle.storage & STM32H755_DMA_HANDLE_INDEX_MASK;
}

static inline uint32_t stm32h755_dma_handle_generation(das_dma_t handle) {
    return handle.storage >> 3u;
}

static inline uint32_t stm32h755_dma_next_generation(uint32_t previous) {
    return previous >= STM32H755_DMA_HANDLE_GENERATION_LIMIT - 1u
        ? UINT32_C(1)
        : previous + 1u;
}

static inline das_dma_t stm32h755_dma_make_handle(uint32_t index,
                                                   uint32_t generation) {
    return (das_dma_t){
        .storage = (generation << 3u) | index
    };
}

/* A token can access a stream only during the precise allocation that issued
 * it. The small generation eventually wraps; tokens must not be retained
 * indefinitely across hundreds of millions of allocations of one stream. */
static inline bool stm32h755_dma_handle_matches(das_dma_t handle,
                                                  uint32_t active_generation,
                                                  bool claimed) {
    const uint32_t token_generation = stm32h755_dma_handle_generation(handle);
    return claimed &&
           handle.storage != UINT32_MAX &&
           token_generation != 0u &&
           token_generation < STM32H755_DMA_HANDLE_GENERATION_LIMIT &&
           token_generation == active_generation;
}

#endif
