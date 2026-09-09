// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_CORTEX_M_TIME_INTERNAL_H
#define DAS_CORTEX_M_TIME_INTERNAL_H

#include <stdint.h>

#include <das/result.h>
#include <das/time.h>

/** Configure the Cortex-M SysTick backend for a 1 kHz monotonic tick. */
das_result_t das_cortex_m_systick_configure(uint32_t core_frequency_hz);

/** Time-source callback exposing the current SysTick millisecond counter. */
das_time_ms_t das_cortex_m_systick_now_ms(void* context);

/** Internal hook called by the weak reusable Cortex-M SysTick handler. */
void das_cortex_m_systick_hook(void);

#endif
