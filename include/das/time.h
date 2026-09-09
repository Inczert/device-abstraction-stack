// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_TIME_H
#define DAS_TIME_H

#include <stdbool.h>
#include <stdint.h>

#include <das/result.h>

/** Millisecond timestamp from the active monotonic time source. */
typedef uint32_t das_time_ms_t;

/** Application/RTOS supplied monotonic millisecond source. */
typedef das_time_ms_t (*das_time_source_fn_t)(void* context);

/**
 * Largest relative interval accepted by deadline helpers.
 *
 * Keeping deadlines inside half of the uint32_t range makes comparisons
 * unambiguous across timestamp wrap-around.
 */
#define DAS_TIME_MAX_INTERVAL_MS UINT32_C(0x7fffffff)

/**
 * Install and start the selected target's default monotonic time source.
 *
 * On the current Cortex-M target this configures SysTick for a 1 kHz tick from
 * the live core frequency. Applications/RTOSes that already own SysTick should
 * use das_time_set_source() instead of calling this function.
 */
das_result_t das_time_init(void);

/**
 * Replace the active time source with an application/RTOS supplied callback.
 *
 * The callback must return a monotonically increasing millisecond timestamp
 * modulo uint32_t wrap. The source/context pair should be changed during
 * initialization, not concurrently with time reads from other contexts.
 */
das_result_t das_time_set_source(das_time_source_fn_t source, void* context);

/** Return true once a default or external time source has been installed. */
bool das_time_is_ready(void);

/**
 * Return the current monotonic millisecond timestamp.
 *
 * Returns zero when no source has been installed. Use das_time_is_ready() when
 * zero is not a sufficient indication of initialization state.
 */
das_time_ms_t das_time_now_ms(void);

/** Return elapsed milliseconds since start_ms using modulo-uint32 arithmetic. */
uint32_t das_time_elapsed_ms(das_time_ms_t start_ms);

/**
 * Return true once interval_ms has elapsed since start_ms.
 *
 * Intervals greater than DAS_TIME_MAX_INTERVAL_MS are rejected as false so the
 * same half-range rule is used consistently by elapsed/deadline helpers.
 */
bool das_time_interval_elapsed(das_time_ms_t start_ms, uint32_t interval_ms);

/**
 * Create a wrap-safe deadline relative to the current monotonic time.
 *
 * delay_ms must be <= DAS_TIME_MAX_INTERVAL_MS.
 */
das_result_t das_time_deadline_after(uint32_t delay_ms,
                                     das_time_ms_t* deadline_ms);

/** Return true when deadline_ms has been reached using wrap-safe comparison. */
bool das_time_deadline_reached(das_time_ms_t deadline_ms);

/**
 * Busy-wait for duration_ms using the active monotonic source.
 *
 * The active source must continue advancing while this function runs. For the
 * default SysTick backend this means interrupts must remain enabled.
 */
das_result_t das_delay_ms(uint32_t duration_ms);

#endif
