// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_CLOCK_H
#define DAS_CLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <das/result.h>

/**
 * Return the number of standard system frequencies supported by the selected
 * board.
 *
 * If frequencies_hz is non-null, up to capacity entries are copied in
 * ascending order. Passing a null buffer or zero capacity is valid and can be
 * used to query the required entry count without allocating memory.
 */
size_t das_clock_get_supported_frequencies(uint32_t* frequencies_hz,
                                           size_t capacity);

/** Return true when frequency_hz is one of the selected board's standard profiles. */
bool das_clock_frequency_supported(uint32_t frequency_hz);

/**
 * Select a standard board system frequency.
 *
 * The caller supplies only the requested frequency. The board/device backend
 * owns oscillator, PLL, voltage, FLASH and bus-divider details.
 *
 * On multi-core devices this selects the primary/system frequency; secondary
 * core and bus frequencies may be derived from it.
 */
das_result_t das_clock_set_frequency(uint32_t frequency_hz);

/** Read the current primary/system frequency derived from live hardware state. */
das_result_t das_clock_get_frequency(uint32_t* frequency_hz);

/**
 * Read the clock frequency of the core executing the current DAS build.
 *
 * This differs from das_clock_get_frequency() on targets where a secondary
 * core runs below the primary/system clock. Drivers such as the default
 * Cortex-M SysTick timebase use this value rather than assuming SYSCLK equals
 * the executing CPU clock.
 */
das_result_t das_clock_get_core_frequency(uint32_t* frequency_hz);

#endif
