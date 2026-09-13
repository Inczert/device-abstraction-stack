// SPDX-License-Identifier: Apache-2.0

#include <das/board_resources.h>
#include <das/cache.h>
#include <das/clock.h>
#include <das/cortex_m/startup.h>
#include <das/dma.h>
#include <das/irq.h>
#include <das/spi.h>
#include <das/time.h>

#include "stm32h755xx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DAS_DMA_TEST_MAGIC UINT32_C(0x444d4139)
#define DAS_DMA_TEST_HARDFAULT UINT32_C(0xe00d0001)

#define DAS_DMA_FLAG_CACHE_API   (UINT32_C(1) << 0u)
#define DAS_DMA_FLAG_HANDLE      (UINT32_C(1) << 1u)
#define DAS_DMA_FLAG_IRQ         (UINT32_C(1) << 2u)
#define DAS_DMA_FLAG_M2M         (UINT32_C(1) << 3u)
#define DAS_DMA_FLAG_STATE       (UINT32_C(1) << 4u)
#define DAS_DMA_FLAG_SPI         (UINT32_C(1) << 5u)
#define DAS_DMA_REQUIRED_FLAGS UINT32_C(0x3f)

#define DAS_DMA_M2M_SIZE 256u
#define DAS_DMA_SPI_SIZE 192u

typedef struct das_dma_test_evidence {
    uint32_t magic;
    volatile uint32_t booted;
    volatile uint32_t error;
    volatile uint32_t heartbeat;
    volatile int32_t clock_result;
    volatile int32_t core_clock_result;
    volatile int32_t time_result;
    volatile uint32_t core_hz;
    volatile uint32_t flags;
    volatile uint32_t cache_available;
    volatile uint32_t cache_enabled;
    volatile uint32_t cache_line_size;
    volatile uint32_t dma_irq;
    volatile uint32_t m2m_bytes;
    volatile uint32_t spi_bytes;
    volatile uint32_t spi_hz;
    volatile uint32_t mismatch_index;
    volatile uint32_t mismatch_expected;
    volatile uint32_t mismatch_actual;
} das_dma_test_evidence_t;

volatile das_dma_test_evidence_t g_das_dma_test_evidence = {
    .magic = DAS_DMA_TEST_MAGIC,
};

static uint8_t g_m2m_source[DAS_DMA_M2M_SIZE] __attribute__((aligned(32)));
static uint8_t g_m2m_destination[DAS_DMA_M2M_SIZE] __attribute__((aligned(32)));
static uint8_t g_spi_tx[DAS_DMA_SPI_SIZE] __attribute__((aligned(32)));
static uint8_t g_spi_rx[DAS_DMA_SPI_SIZE] __attribute__((aligned(32)));

static void stop_with_error(uint32_t error) {
    g_das_dma_test_evidence.error = error;
    for (;;) { __NOP(); }
}

static void make_pattern(uint8_t* data, size_t size, uint8_t seed) {
    for (size_t index = 0u; index < size; ++index) {
        data[index] = (uint8_t)(((uint32_t)index * UINT32_C(53) + seed) & UINT32_C(0xff));
    }
}

static void fill_bytes(uint8_t* data, size_t size, uint8_t value) {
    for (size_t index = 0u; index < size; ++index) data[index] = value;
}

static bool compare_bytes(const uint8_t* expected,
                          const uint8_t* actual,
                          size_t size) {
    for (size_t index = 0u; index < size; ++index) {
        if (actual[index] != expected[index]) {
            g_das_dma_test_evidence.mismatch_index = (uint32_t)index;
            g_das_dma_test_evidence.mismatch_expected = expected[index];
            g_das_dma_test_evidence.mismatch_actual = actual[index];
            return false;
        }
    }
    return true;
}

static void qualify_cache_api(void) {
    g_das_dma_test_evidence.cache_available = das_cache_data_available() ? 1u : 0u;
    g_das_dma_test_evidence.cache_line_size = (uint32_t)das_cache_data_line_size();

#if defined(CORE_CM7)
    if (!das_cache_data_available() || das_cache_data_line_size() != 32u) {
        stop_with_error(UINT32_C(0x0d10));
    }
    if (das_cache_data_enable() != DAS_OK || !das_cache_data_is_enabled()) {
        stop_with_error(UINT32_C(0x0d11));
    }
#else
    if (das_cache_data_available() || das_cache_data_line_size() != 0u) {
        stop_with_error(UINT32_C(0x0d12));
    }
    if (das_cache_data_enable() != DAS_ERROR_UNSUPPORTED || das_cache_data_is_enabled()) {
        stop_with_error(UINT32_C(0x0d13));
    }
#endif

    g_das_dma_test_evidence.cache_enabled = das_cache_data_is_enabled() ? 1u : 0u;
    g_das_dma_test_evidence.flags |= DAS_DMA_FLAG_CACHE_API;
}

static void qualify_memory_to_memory(void) {
    make_pattern(g_m2m_source, sizeof(g_m2m_source), UINT8_C(0x31));
    fill_bytes(g_m2m_destination, sizeof(g_m2m_destination), UINT8_C(0xa5));

    if (das_cache_data_clean(g_m2m_source, sizeof(g_m2m_source)) != DAS_OK ||
        das_cache_data_clean_invalidate(g_m2m_destination,
                                        sizeof(g_m2m_destination)) != DAS_OK) {
        stop_with_error(UINT32_C(0x0d20));
    }

    das_dma_t dma = DAS_DMA_INVALID;
    das_result_t result = das_dma_acquire(&dma);
    if (result != DAS_OK || !das_dma_is_valid(dma)) {
        stop_with_error(UINT32_C(0x0d21) | (uint32_t)(-result & 0x0f));
    }
    g_das_dma_test_evidence.flags |= DAS_DMA_FLAG_HANDLE;

    das_irq_t irq = DAS_IRQ_INVALID;
    result = das_dma_get_irq(dma, &irq);
    if (result != DAS_OK || !das_irq_is_valid(irq)) {
        stop_with_error(UINT32_C(0x0d30) | (uint32_t)(-result & 0x0f));
    }
    g_das_dma_test_evidence.dma_irq = irq.storage;
    g_das_dma_test_evidence.flags |= DAS_DMA_FLAG_IRQ;

    const das_dma_config_t config = {
        .direction = DAS_DMA_MEMORY_TO_MEMORY,
        .source_width = DAS_DMA_WIDTH_BYTE,
        .destination_width = DAS_DMA_WIDTH_BYTE,
        .source_increment = true,
        .destination_increment = true,
    };
    result = das_dma_configure(dma, &config);
    if (result != DAS_OK) {
        stop_with_error(UINT32_C(0x0d40) | (uint32_t)(-result & 0x0f));
    }
    result = das_dma_start(dma,
                           g_m2m_source,
                           g_m2m_destination,
                           sizeof(g_m2m_source));
    if (result != DAS_OK) {
        stop_with_error(UINT32_C(0x0d50) | (uint32_t)(-result & 0x0f));
    }
    result = das_dma_wait_timeout(dma, 50u);
    if (result != DAS_OK) {
        stop_with_error(UINT32_C(0x0d60) | (uint32_t)(-result & 0x0f));
    }

    das_dma_state_t state = DAS_DMA_STATE_IDLE;
    size_t remaining = 1u;
    if (das_dma_get_state(dma, &state) != DAS_OK ||
        das_dma_get_remaining(dma, &remaining) != DAS_OK ||
        state != DAS_DMA_STATE_COMPLETE || remaining != 0u) {
        stop_with_error(UINT32_C(0x0d70));
    }
    g_das_dma_test_evidence.flags |= DAS_DMA_FLAG_STATE;

    if (das_cache_data_invalidate(g_m2m_destination,
                                  sizeof(g_m2m_destination)) != DAS_OK ||
        !compare_bytes(g_m2m_source, g_m2m_destination, sizeof(g_m2m_source))) {
        stop_with_error(UINT32_C(0x0d80));
    }
    g_das_dma_test_evidence.m2m_bytes = sizeof(g_m2m_source);
    g_das_dma_test_evidence.flags |= DAS_DMA_FLAG_M2M;

    result = das_dma_release(dma);
    if (result != DAS_OK || das_dma_is_valid(dma)) {
        stop_with_error(UINT32_C(0x0d90) | (uint32_t)(-result & 0x0f));
    }
}

static void qualify_spi_dma(void) {
    const das_spi_config_t config = {
        .frequency_hz = UINT32_C(4000000),
        .mode = DAS_SPI_MODE_0,
        .bit_order = DAS_SPI_MSB_FIRST,
    };
    das_spi_t spi = DAS_SPI_INVALID;
    das_result_t result = das_board_spi_init(DAS_BOARD_SPI_ARDUINO, &config, &spi);
    if (result != DAS_OK || !das_spi_is_valid(spi)) {
        stop_with_error(UINT32_C(0x0da0) | (uint32_t)(-result & 0x0f));
    }
    result = das_spi_get_frequency(spi, (uint32_t*)&g_das_dma_test_evidence.spi_hz);
    if (result != DAS_OK || g_das_dma_test_evidence.spi_hz != UINT32_C(4000000)) {
        stop_with_error(UINT32_C(0x0db0) | (uint32_t)(-result & 0x0f));
    }

    make_pattern(g_spi_tx, sizeof(g_spi_tx), UINT8_C(0x67));
    fill_bytes(g_spi_rx, sizeof(g_spi_rx), UINT8_C(0x5a));
    if (das_cache_data_clean(g_spi_tx, sizeof(g_spi_tx)) != DAS_OK ||
        das_cache_data_clean_invalidate(g_spi_rx, sizeof(g_spi_rx)) != DAS_OK) {
        stop_with_error(UINT32_C(0x0dc0));
    }

    result = das_board_spi_chip_select(DAS_BOARD_SPI_ARDUINO, true);
    if (result != DAS_OK) stop_with_error(UINT32_C(0x0dd0) | (uint32_t)(-result & 0x0f));
    result = das_spi_transfer_dma_timeout(spi,
                                          g_spi_tx,
                                          g_spi_rx,
                                          sizeof(g_spi_tx),
                                          50u);
    const das_result_t cs_result =
        das_board_spi_chip_select(DAS_BOARD_SPI_ARDUINO, false);
    if (result != DAS_OK || cs_result != DAS_OK) {
        stop_with_error(UINT32_C(0x0de0) | (uint32_t)(-result & 0x0f));
    }

    if (das_cache_data_invalidate(g_spi_rx, sizeof(g_spi_rx)) != DAS_OK ||
        !compare_bytes(g_spi_tx, g_spi_rx, sizeof(g_spi_tx))) {
        stop_with_error(UINT32_C(0x0df0));
    }
    g_das_dma_test_evidence.spi_bytes = sizeof(g_spi_tx);
    g_das_dma_test_evidence.flags |= DAS_DMA_FLAG_SPI;
}

int main(void) {
#if defined(CORE_CM7)
    g_das_dma_test_evidence.clock_result =
        das_clock_set_frequency(UINT32_C(400000000));
#else
    g_das_dma_test_evidence.clock_result = DAS_OK;
#endif
    if (g_das_dma_test_evidence.clock_result != DAS_OK) {
        stop_with_error(UINT32_C(0x0d01));
    }

    uint32_t core_hz = 0u;
    g_das_dma_test_evidence.core_clock_result = das_clock_get_core_frequency(&core_hz);
    g_das_dma_test_evidence.core_hz = core_hz;
    if (g_das_dma_test_evidence.core_clock_result != DAS_OK) {
        stop_with_error(UINT32_C(0x0d02));
    }

    g_das_dma_test_evidence.time_result = das_time_init();
    if (g_das_dma_test_evidence.time_result != DAS_OK) {
        stop_with_error(UINT32_C(0x0d03));
    }

    qualify_cache_api();
    qualify_memory_to_memory();
    qualify_spi_dma();

    if (g_das_dma_test_evidence.flags != DAS_DMA_REQUIRED_FLAGS) {
        stop_with_error(UINT32_C(0x0dff));
    }

    g_das_dma_test_evidence.booted = 1u;
    for (;;) {
        ++g_das_dma_test_evidence.heartbeat;
    }
}

void HardFault_Handler(void) {
    g_das_dma_test_evidence.error = DAS_DMA_TEST_HARDFAULT;
    for (;;) { __NOP(); }
}
