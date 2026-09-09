// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_STM32H755_CLOCK_INTERNAL_H
#define DAS_STM32H755_CLOCK_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include <das/result.h>

/** STM32H755 system-clock source. HSE electrical availability is board policy. */
typedef enum stm32h755_clock_source {
    STM32H755_CLOCK_SOURCE_HSI = 0,
    STM32H755_CLOCK_SOURCE_HSE,
    STM32H755_CLOCK_SOURCE_HSE_BYPASS
} stm32h755_clock_source_t;

/** Device-level clock configuration consumed by the STM32H755 backend. */
typedef struct stm32h755_clock_config {
    stm32h755_clock_source_t source;
    /** HSE frequency in hertz. Ignored for HSI, which is fixed at 64 MHz. */
    uint32_t source_hz;
    bool use_pll1;
    uint8_t pll_m;
    uint16_t pll_n;
    uint8_t pll_p;
    uint8_t pll_q;
    uint8_t pll_r;
    uint16_t d1_core_divider;
    uint16_t ahb_divider;
    uint8_t apb1_divider;
    uint8_t apb2_divider;
    uint8_t apb3_divider;
    uint8_t apb4_divider;
    /** Maximum register polls for each oscillator/power/clock transition. */
    uint32_t wait_limit;
} stm32h755_clock_config_t;

/** Effective clocks derived from live RCC registers. */
typedef struct stm32h755_clock_frequencies {
    uint32_t system_hz;
    uint32_t cm7_hz;
    uint32_t cm4_hz;
    uint32_t ahb_hz;
    uint32_t apb1_hz;
    uint32_t apb2_hz;
    uint32_t apb3_hz;
    uint32_t apb4_hz;
} stm32h755_clock_frequencies_t;

/**
 * Apply a managed STM32H755 system-clock configuration.
 *
 * CPU1/CM7 owns global system-clock changes. CM4 builds return
 * DAS_ERROR_UNSUPPORTED. The current managed envelope uses VOS1 and four FLASH
 * wait states and accepts configurations up to 400 MHz CM7, 200 MHz HCLK/CM4,
 * and 100 MHz on APB1..4.
 */
das_result_t stm32h755_clock_apply(const stm32h755_clock_config_t* config);

/**
 * Derive the live clock tree from RCC registers.
 *
 * hse_frequency_hz is required only when the current clock path depends on HSE;
 * the MCU registers cannot encode the physical external oscillator frequency.
 */
das_result_t stm32h755_clock_get_frequencies(
    uint32_t hse_frequency_hz,
    stm32h755_clock_frequencies_t* frequencies);

#endif
