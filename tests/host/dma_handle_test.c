// SPDX-License-Identifier: Apache-2.0

#include "dma_handle_internal.h"

#include <das/dma.h>

#include <stdbool.h>
#include <stdint.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

/* Host regression exercises the exact token encoder/validator compiled into
 * the device driver, without pretending host memory models STM32 DMA hardware.
 * Actual stream acquire/release checks also run in the physical DMA fixture. */
int main(void) {
    const das_dma_t invalid = DAS_DMA_INVALID;
    CHECK(!stm32h755_dma_handle_matches(invalid, 1u, true));
    CHECK(!stm32h755_dma_handle_matches((das_dma_t){0u}, 0u, true));
    CHECK(stm32h755_dma_next_generation(0u) == 1u);

    for (uint32_t index = 0u; index < 8u; ++index) {
        const uint32_t first_generation = stm32h755_dma_next_generation(0u);
        const das_dma_t stale =
            stm32h755_dma_make_handle(index, first_generation);
        CHECK(stm32h755_dma_handle_index(stale) == index);
        CHECK(stm32h755_dma_handle_generation(stale) == first_generation);
        CHECK(stm32h755_dma_handle_matches(stale, first_generation, true));
        CHECK(!stm32h755_dma_handle_matches(stale, first_generation, false));

        const uint32_t second_generation =
            stm32h755_dma_next_generation(first_generation);
        const das_dma_t current =
            stm32h755_dma_make_handle(index, second_generation);
        CHECK(current.storage != stale.storage);
        CHECK(current.storage != UINT32_MAX);
        CHECK(!stm32h755_dma_handle_matches(stale, second_generation, true));
        CHECK(stm32h755_dma_handle_matches(current, second_generation, true));
    }

    const uint32_t last = STM32H755_DMA_HANDLE_GENERATION_LIMIT - 1u;
    CHECK(stm32h755_dma_next_generation(last) == 1u);
    for (uint32_t index = 0u; index < 8u; ++index) {
        const das_dma_t maximum = stm32h755_dma_make_handle(index, last);
        CHECK(maximum.storage != UINT32_MAX);
        CHECK(stm32h755_dma_handle_matches(maximum, last, true));
        CHECK(!stm32h755_dma_handle_matches(maximum, 1u, true));
    }
    return 0;
}
