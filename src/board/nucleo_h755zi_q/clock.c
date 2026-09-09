// SPDX-License-Identifier: Apache-2.0

#include <das/clock.h>

#include "clock_internal.h"
#include "power_internal.h"

#include <stddef.h>
#include <stdint.h>

#define DAS_NUCLEO_H755ZI_Q_WAIT_LIMIT UINT32_C(1000000)

static const uint32_t SUPPORTED_FREQUENCIES_HZ[] = {
    UINT32_C(64000000),
    UINT32_C(200000000),
    UINT32_C(300000000),
    UINT32_C(400000000),
};

#if defined(CORE_CM7)
static bool build_profile(uint32_t frequency_hz,
                          stm32h755_clock_config_t* config) {
    if (config == 0) {
        return false;
    }

    *config = (stm32h755_clock_config_t){
        .source = STM32H755_CLOCK_SOURCE_HSI,
        .source_hz = 0u,
        .use_pll1 = false,
        .pll_m = 1u,
        .pll_n = 4u,
        .pll_p = 2u,
        .pll_q = 2u,
        .pll_r = 2u,
        .d1_core_divider = 1u,
        .ahb_divider = 1u,
        .apb1_divider = 1u,
        .apb2_divider = 1u,
        .apb3_divider = 1u,
        .apb4_divider = 1u,
        .wait_limit = DAS_NUCLEO_H755ZI_Q_WAIT_LIMIT,
    };

    switch (frequency_hz) {
        case UINT32_C(64000000):
            return true;

        case UINT32_C(200000000):
            config->use_pll1 = true;
            config->pll_m = 8u;
            config->pll_n = 50u;
            config->pll_p = 2u;
            config->pll_q = 4u;
            config->pll_r = 2u;
            config->ahb_divider = 1u;
            config->apb1_divider = 2u;
            config->apb2_divider = 2u;
            config->apb3_divider = 2u;
            config->apb4_divider = 2u;
            return true;

        case UINT32_C(300000000):
            config->use_pll1 = true;
            config->pll_m = 8u;
            config->pll_n = 75u;
            config->pll_p = 2u;
            config->pll_q = 4u;
            config->pll_r = 2u;
            config->ahb_divider = 2u;
            config->apb1_divider = 2u;
            config->apb2_divider = 2u;
            config->apb3_divider = 2u;
            config->apb4_divider = 2u;
            return true;

        case UINT32_C(400000000):
            config->use_pll1 = true;
            config->pll_m = 8u;
            config->pll_n = 100u;
            config->pll_p = 2u;
            config->pll_q = 4u;
            config->pll_r = 2u;
            config->ahb_divider = 2u;
            config->apb1_divider = 2u;
            config->apb2_divider = 2u;
            config->apb3_divider = 2u;
            config->apb4_divider = 2u;
            return true;

        default:
            return false;
    }
}
#endif

size_t das_clock_get_supported_frequencies(uint32_t* frequencies_hz,
                                           size_t capacity) {
    const size_t count =
        sizeof(SUPPORTED_FREQUENCIES_HZ) / sizeof(SUPPORTED_FREQUENCIES_HZ[0]);

    if (frequencies_hz != 0 && capacity != 0u) {
        const size_t copy_count = capacity < count ? capacity : count;
        for (size_t index = 0u; index < copy_count; ++index) {
            frequencies_hz[index] = SUPPORTED_FREQUENCIES_HZ[index];
        }
    }

    return count;
}

bool das_clock_frequency_supported(uint32_t frequency_hz) {
    const size_t count =
        sizeof(SUPPORTED_FREQUENCIES_HZ) / sizeof(SUPPORTED_FREQUENCIES_HZ[0]);

    for (size_t index = 0u; index < count; ++index) {
        if (SUPPORTED_FREQUENCIES_HZ[index] == frequency_hz) {
            return true;
        }
    }
    return false;
}

das_result_t das_clock_set_frequency(uint32_t frequency_hz) {
#if defined(CORE_CM4)
    (void)frequency_hz;
    return DAS_ERROR_UNSUPPORTED;
#elif defined(CORE_CM7)
    stm32h755_clock_config_t config = {0};
    if (!build_profile(frequency_hz, &config)) {
        return DAS_ERROR_UNSUPPORTED;
    }

    das_result_t result =
        stm32h755_power_configure_direct_smps(config.wait_limit);
    if (result != DAS_OK) {
        return result;
    }

    return stm32h755_clock_apply(&config);
#else
#error "NUCLEO-H755ZI-Q clock backend requires CORE_CM7 or CORE_CM4"
#endif
}

das_result_t das_clock_get_frequency(uint32_t* frequency_hz) {
    if (frequency_hz == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    stm32h755_clock_frequencies_t frequencies = {0};
    const das_result_t result =
        stm32h755_clock_get_frequencies(0u, &frequencies);
    if (result != DAS_OK) {
        return result;
    }

    *frequency_hz = frequencies.system_hz;
    return DAS_OK;
}

das_result_t das_clock_get_core_frequency(uint32_t* frequency_hz) {
    if (frequency_hz == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    stm32h755_clock_frequencies_t frequencies = {0};
    const das_result_t result =
        stm32h755_clock_get_frequencies(0u, &frequencies);
    if (result != DAS_OK) {
        return result;
    }

#if defined(CORE_CM7)
    *frequency_hz = frequencies.cm7_hz;
#elif defined(CORE_CM4)
    *frequency_hz = frequencies.cm4_hz;
#else
#error "NUCLEO-H755ZI-Q clock backend requires CORE_CM7 or CORE_CM4"
#endif
    return DAS_OK;
}
