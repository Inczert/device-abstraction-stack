// SPDX-License-Identifier: Apache-2.0

#include "time_internal.h"

#ifndef DAS_CMSIS_DEVICE_HEADER
#error "Cortex-M time backend requires a CMSIS device header"
#endif

#include DAS_CMSIS_DEVICE_HEADER

#include <stdint.h>

#define DAS_CORTEX_M_SYSTICK_HZ UINT32_C(1000)

static volatile das_time_ms_t g_systick_ms;

das_result_t das_cortex_m_systick_configure(uint32_t core_frequency_hz) {
    if (core_frequency_hz == 0u ||
        (core_frequency_hz % DAS_CORTEX_M_SYSTICK_HZ) != 0u) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    const uint32_t ticks = core_frequency_hz / DAS_CORTEX_M_SYSTICK_HZ;
    if (ticks == 0u || (ticks - 1u) > SysTick_LOAD_RELOAD_Msk) {
        return DAS_ERROR_UNSUPPORTED;
    }

    /* Reconfiguration preserves the monotonic counter itself. */
    SysTick->CTRL = 0u;
    SysTick->VAL = 0u;
    if (SysTick_Config(ticks) != 0u) {
        return DAS_ERROR_UNSUPPORTED;
    }

    __DSB();
    __ISB();
    return DAS_OK;
}

das_time_ms_t das_cortex_m_systick_now_ms(void* context) {
    (void)context;
    return g_systick_ms;
}

void das_cortex_m_systick_hook(void) {
    ++g_systick_ms;
}
