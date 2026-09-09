// SPDX-License-Identifier: Apache-2.0

#include <das/clock.h>
#include <das/time.h>

#include "time_internal.h"

#include <stdint.h>

das_result_t das_time_init(void) {
    uint32_t core_frequency_hz = 0u;
    das_result_t result = das_clock_get_core_frequency(&core_frequency_hz);
    if (result != DAS_OK) {
        return result;
    }

    result = das_cortex_m_systick_configure(core_frequency_hz);
    if (result != DAS_OK) {
        return result;
    }

    return das_time_set_source(das_cortex_m_systick_now_ms, 0);
}
