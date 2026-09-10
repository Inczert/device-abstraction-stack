// SPDX-License-Identifier: Apache-2.0

#include <das/dma.h>
#include <das/time.h>

#include "dma_internal.h"
#include "stm32h755xx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DAS_STM32H755_DMA_STREAM_COUNT 8u
#define DAS_STM32H755_DMA_MAX_COUNT UINT32_C(0xffff)
#define DAS_STM32H755_DMA_ALL_FLAGS UINT32_C(0x3d)
#define DAS_STM32H755_DMA_ERROR_FLAGS UINT32_C(0x0d)
#define DAS_STM32H755_DMA_COMPLETE_FLAG UINT32_C(0x20)
#define DAS_STM32H755_DMA_DISABLE_SPINS UINT32_C(100000)

#if defined(CORE_CM7)
#define DAS_RCC_CORE RCC_C1
#elif defined(CORE_CM4)
#define DAS_RCC_CORE RCC_C2
#else
#error "STM32H755 DMA backend requires CORE_CM7 or CORE_CM4"
#endif

static bool g_dma_claimed[DAS_STM32H755_DMA_STREAM_COUNT];
static bool g_dma_configured[DAS_STM32H755_DMA_STREAM_COUNT];
static das_dma_config_t g_dma_config[DAS_STM32H755_DMA_STREAM_COUNT];

static DMA_Stream_TypeDef* stream_from_index(uint32_t index) {
    switch (index) {
        case 0u: return DMA1_Stream0;
        case 1u: return DMA1_Stream1;
        case 2u: return DMA1_Stream2;
        case 3u: return DMA1_Stream3;
        case 4u: return DMA1_Stream4;
        case 5u: return DMA1_Stream5;
        case 6u: return DMA1_Stream6;
        case 7u: return DMA1_Stream7;
        default: return 0;
    }
}

static DMAMUX_Channel_TypeDef* dmamux_from_index(uint32_t index) {
    switch (index) {
        case 0u: return DMAMUX1_Channel0;
        case 1u: return DMAMUX1_Channel1;
        case 2u: return DMAMUX1_Channel2;
        case 3u: return DMAMUX1_Channel3;
        case 4u: return DMAMUX1_Channel4;
        case 5u: return DMAMUX1_Channel5;
        case 6u: return DMAMUX1_Channel6;
        case 7u: return DMAMUX1_Channel7;
        default: return 0;
    }
}

static IRQn_Type irq_from_index(uint32_t index) {
    switch (index) {
        case 0u: return DMA1_Stream0_IRQn;
        case 1u: return DMA1_Stream1_IRQn;
        case 2u: return DMA1_Stream2_IRQn;
        case 3u: return DMA1_Stream3_IRQn;
        case 4u: return DMA1_Stream4_IRQn;
        case 5u: return DMA1_Stream5_IRQn;
        case 6u: return DMA1_Stream6_IRQn;
        default: return DMA1_Stream7_IRQn;
    }
}

static bool handle_index(das_dma_t dma, uint32_t* index) {
    if (dma.storage >= DAS_STM32H755_DMA_STREAM_COUNT ||
        !g_dma_claimed[dma.storage]) {
        return false;
    }
    if (index != 0) *index = dma.storage;
    return true;
}

static uint32_t flag_shift(uint32_t index) {
    static const uint8_t shifts[4] = {0u, 6u, 16u, 22u};
    return shifts[index & 3u];
}

static uint32_t stream_flags(uint32_t index) {
    const uint32_t status = index < 4u ? DMA1->LISR : DMA1->HISR;
    return (status >> flag_shift(index)) & DAS_STM32H755_DMA_ALL_FLAGS;
}

static void clear_stream_flags(uint32_t index) {
    const uint32_t flags = DAS_STM32H755_DMA_ALL_FLAGS << flag_shift(index);
    if (index < 4u) {
        DMA1->LIFCR = flags;
    } else {
        DMA1->HIFCR = flags;
    }
    DMAMUX1_ChannelStatus->CFR = UINT32_C(1) << index;
}

static void enable_dma_clock(void) {
    DAS_RCC_CORE->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    (void)DAS_RCC_CORE->AHB1ENR;
    __DSB();
}

static bool width_valid(das_dma_width_t width) {
    return width == DAS_DMA_WIDTH_BYTE ||
           width == DAS_DMA_WIDTH_HALFWORD ||
           width == DAS_DMA_WIDTH_WORD;
}

static uint32_t width_code(das_dma_width_t width) {
    switch (width) {
        case DAS_DMA_WIDTH_HALFWORD: return 1u;
        case DAS_DMA_WIDTH_WORD: return 2u;
        default: return 0u;
    }
}

static bool config_valid(const das_dma_config_t* config) {
    if (config == 0 || !width_valid(config->source_width) ||
        !width_valid(config->destination_width)) {
        return false;
    }
    if ((unsigned)config->direction > (unsigned)DAS_DMA_MEMORY_TO_MEMORY) {
        return false;
    }

    /* Direct mode is used by the baseline; equal widths avoid packing rules. */
    return config->source_width == config->destination_width;
}

static bool address_aligned(const void* address, das_dma_width_t width) {
    return (((uintptr_t)address) & ((uintptr_t)width - 1u)) == 0u;
}

static das_result_t stop_stream(uint32_t index) {
    DMA_Stream_TypeDef* const stream = stream_from_index(index);
    stream->CR &= ~DMA_SxCR_EN;
    __DSB();
    for (uint32_t spin = 0u; spin < DAS_STM32H755_DMA_DISABLE_SPINS; ++spin) {
        if ((stream->CR & DMA_SxCR_EN) == 0u) return DAS_OK;
    }
    return DAS_ERROR_TIMEOUT;
}

das_result_t stm32h755_dma_set_request(das_dma_t dma, uint32_t request) {
    uint32_t index = 0u;
    if (!handle_index(dma, &index)) return DAS_ERROR_INVALID_ARGUMENT;
    if ((request & ~DMAMUX_CxCR_DMAREQ_ID) != 0u) return DAS_ERROR_INVALID_ARGUMENT;

    DMA_Stream_TypeDef* const stream = stream_from_index(index);
    if ((stream->CR & DMA_SxCR_EN) != 0u) return DAS_ERROR_NOT_READY;

    DMAMUX_Channel_TypeDef* const dmamux = dmamux_from_index(index);
    dmamux->CCR = (dmamux->CCR & ~DMAMUX_CxCR_DMAREQ_ID) |
                  (request & DMAMUX_CxCR_DMAREQ_ID);
    DMAMUX1_ChannelStatus->CFR = UINT32_C(1) << index;
    __DSB();
    return DAS_OK;
}

das_result_t das_dma_acquire(das_dma_t* dma) {
    if (dma == 0) return DAS_ERROR_INVALID_ARGUMENT;
    enable_dma_clock();

    for (uint32_t index = 0u; index < DAS_STM32H755_DMA_STREAM_COUNT; ++index) {
        if (!g_dma_claimed[index]) {
            g_dma_claimed[index] = true;
            g_dma_configured[index] = false;
            *dma = (das_dma_t){.storage = index};
            clear_stream_flags(index);
            dmamux_from_index(index)->CCR = 0u;
            return DAS_OK;
        }
    }

    *dma = DAS_DMA_INVALID;
    return DAS_ERROR_NOT_READY;
}

das_result_t das_dma_release(das_dma_t dma) {
    uint32_t index = 0u;
    if (!handle_index(dma, &index)) return DAS_ERROR_INVALID_ARGUMENT;

    const das_result_t result = stop_stream(index);
    if (result != DAS_OK) return result;
    clear_stream_flags(index);
    dmamux_from_index(index)->CCR = 0u;
    g_dma_configured[index] = false;
    g_dma_claimed[index] = false;
    return DAS_OK;
}

bool das_dma_is_valid(das_dma_t dma) {
    return handle_index(dma, 0);
}

das_result_t das_dma_configure(das_dma_t dma, const das_dma_config_t* config) {
    uint32_t index = 0u;
    if (!handle_index(dma, &index) || !config_valid(config)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    DMA_Stream_TypeDef* const stream = stream_from_index(index);
    if ((stream->CR & DMA_SxCR_EN) != 0u) return DAS_ERROR_NOT_READY;

    uint32_t cr = DMA_SxCR_PL_1;
    switch (config->direction) {
        case DAS_DMA_PERIPHERAL_TO_MEMORY:
            if (config->source_increment) cr |= DMA_SxCR_PINC;
            if (config->destination_increment) cr |= DMA_SxCR_MINC;
            cr |= width_code(config->source_width) << DMA_SxCR_PSIZE_Pos;
            cr |= width_code(config->destination_width) << DMA_SxCR_MSIZE_Pos;
            break;
        case DAS_DMA_MEMORY_TO_PERIPHERAL:
            cr |= DMA_SxCR_DIR_0;
            if (config->source_increment) cr |= DMA_SxCR_MINC;
            if (config->destination_increment) cr |= DMA_SxCR_PINC;
            cr |= width_code(config->source_width) << DMA_SxCR_MSIZE_Pos;
            cr |= width_code(config->destination_width) << DMA_SxCR_PSIZE_Pos;
            break;
        case DAS_DMA_MEMORY_TO_MEMORY:
            cr |= DMA_SxCR_DIR_1;
            if (config->source_increment) cr |= DMA_SxCR_PINC;
            if (config->destination_increment) cr |= DMA_SxCR_MINC;
            cr |= width_code(config->source_width) << DMA_SxCR_PSIZE_Pos;
            cr |= width_code(config->destination_width) << DMA_SxCR_MSIZE_Pos;
            break;
        default:
            return DAS_ERROR_INVALID_ARGUMENT;
    }

    stream->CR = cr;
    stream->NDTR = 0u;
    stream->PAR = 0u;
    stream->M0AR = 0u;
    stream->M1AR = 0u;
    stream->FCR = 0u;
    clear_stream_flags(index);
    g_dma_config[index] = *config;
    g_dma_configured[index] = true;
    return stm32h755_dma_set_request(dma, STM32H755_DMA_REQUEST_MEM2MEM);
}

das_result_t das_dma_start(das_dma_t dma,
                           const void* source,
                           void* destination,
                           size_t count) {
    uint32_t index = 0u;
    if (!handle_index(dma, &index) || !g_dma_configured[index] ||
        source == 0 || destination == 0 || count == 0u) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (count > DAS_STM32H755_DMA_MAX_COUNT) return DAS_ERROR_INVALID_ARGUMENT;

    const das_dma_config_t* const config = &g_dma_config[index];
    if (!address_aligned(source, config->source_width) ||
        !address_aligned(destination, config->destination_width)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    DMA_Stream_TypeDef* const stream = stream_from_index(index);
    if ((stream->CR & DMA_SxCR_EN) != 0u) return DAS_ERROR_NOT_READY;

    clear_stream_flags(index);
    stream->NDTR = (uint32_t)count;
    switch (config->direction) {
        case DAS_DMA_PERIPHERAL_TO_MEMORY:
            stream->PAR = (uint32_t)(uintptr_t)source;
            stream->M0AR = (uint32_t)(uintptr_t)destination;
            break;
        case DAS_DMA_MEMORY_TO_PERIPHERAL:
            stream->PAR = (uint32_t)(uintptr_t)destination;
            stream->M0AR = (uint32_t)(uintptr_t)source;
            break;
        case DAS_DMA_MEMORY_TO_MEMORY:
            stream->PAR = (uint32_t)(uintptr_t)source;
            stream->M0AR = (uint32_t)(uintptr_t)destination;
            break;
        default:
            return DAS_ERROR_INVALID_ARGUMENT;
    }

    __DSB();
    stream->CR |= DMA_SxCR_EN;
    return DAS_OK;
}

das_result_t das_dma_get_state(das_dma_t dma, das_dma_state_t* state) {
    uint32_t index = 0u;
    if (!handle_index(dma, &index) || state == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    const uint32_t flags = stream_flags(index);
    if ((flags & DAS_STM32H755_DMA_ERROR_FLAGS) != 0u) {
        *state = DAS_DMA_STATE_ERROR;
    } else if ((flags & DAS_STM32H755_DMA_COMPLETE_FLAG) != 0u) {
        *state = DAS_DMA_STATE_COMPLETE;
    } else if ((stream_from_index(index)->CR & DMA_SxCR_EN) != 0u) {
        *state = DAS_DMA_STATE_BUSY;
    } else {
        *state = DAS_DMA_STATE_IDLE;
    }
    return DAS_OK;
}

das_result_t das_dma_get_remaining(das_dma_t dma, size_t* remaining) {
    uint32_t index = 0u;
    if (!handle_index(dma, &index) || remaining == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    *remaining = (size_t)stream_from_index(index)->NDTR;
    return DAS_OK;
}

das_result_t das_dma_wait_timeout(das_dma_t dma, uint32_t timeout_ms) {
    if (!das_dma_is_valid(dma)) return DAS_ERROR_INVALID_ARGUMENT;

    const bool finite = timeout_ms != DAS_DMA_WAIT_FOREVER;
    das_time_ms_t start = 0u;
    if (finite) {
        if (timeout_ms > DAS_TIME_MAX_INTERVAL_MS) return DAS_ERROR_INVALID_ARGUMENT;
        if (!das_time_is_ready()) return DAS_ERROR_NOT_READY;
        start = das_time_now_ms();
    }

    for (;;) {
        das_dma_state_t state = DAS_DMA_STATE_IDLE;
        das_result_t result = das_dma_get_state(dma, &state);
        if (result != DAS_OK) return result;
        if (state == DAS_DMA_STATE_COMPLETE) return DAS_OK;
        if (state == DAS_DMA_STATE_ERROR) return DAS_ERROR_IO;
        if (state == DAS_DMA_STATE_IDLE) return DAS_ERROR_NOT_READY;
        if (finite && das_time_interval_elapsed(start, timeout_ms)) {
            (void)das_dma_abort(dma);
            return DAS_ERROR_TIMEOUT;
        }
    }
}

das_result_t das_dma_wait(das_dma_t dma) {
    return das_dma_wait_timeout(dma, DAS_DMA_WAIT_FOREVER);
}

das_result_t das_dma_abort(das_dma_t dma) {
    uint32_t index = 0u;
    if (!handle_index(dma, &index)) return DAS_ERROR_INVALID_ARGUMENT;
    const das_result_t result = stop_stream(index);
    if (result != DAS_OK) return result;
    clear_stream_flags(index);
    return DAS_OK;
}

das_result_t das_dma_get_irq(das_dma_t dma, das_irq_t* irq) {
    uint32_t index = 0u;
    if (!handle_index(dma, &index) || irq == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    *irq = (das_irq_t){.storage = (uint32_t)irq_from_index(index)};
    return DAS_OK;
}
