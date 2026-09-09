// SPDX-License-Identifier: Apache-2.0

#include "power_internal.h"
#include "stm32h755xx.h"

#include <stdbool.h>
#include <stdint.h>

#if defined(CORE_CM7)

static das_result_t wait_mask(volatile uint32_t* reg,
                              uint32_t mask,
                              bool set,
                              uint32_t wait_limit) {
    for (uint32_t poll = 0u; poll < wait_limit; ++poll) {
        const bool observed = (*reg & mask) != 0u;
        if (observed == set) {
            return DAS_OK;
        }
    }
    return DAS_ERROR_TIMEOUT;
}

#endif

das_result_t stm32h755_power_configure_direct_smps(uint32_t wait_limit) {
#if defined(CORE_CM4)
    (void)wait_limit;
    return DAS_ERROR_UNSUPPORTED;
#elif defined(CORE_CM7)
    if (wait_limit == 0u) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    /*
     * After POR the STM32H755 starts in Run* mode with both SMPS and LDO
     * enabled while the supply configuration is still writable. The selected
     * board must confirm that direct SMPS matches its physical wiring.
     *
     * Once supply configuration is locked, only accept an already matching
     * direct-SMPS state. Do not attempt to rewrite an incompatible power path.
     */
    const uint32_t run_star_state = PWR_CR3_SMPSEN | PWR_CR3_LDOEN;
    const uint32_t supply_state =
        PWR->CR3 & (PWR_CR3_SMPSEN | PWR_CR3_LDOEN | PWR_CR3_BYPASS);

    if (supply_state == run_star_state) {
        PWR->CR3 &= ~PWR_CR3_LDOEN;
        (void)PWR->CR3;
    } else if (supply_state != PWR_CR3_SMPSEN) {
        return DAS_ERROR_UNSUPPORTED;
    }

    if ((PWR->CR3 & (PWR_CR3_SMPSEN | PWR_CR3_LDOEN | PWR_CR3_BYPASS)) !=
        PWR_CR3_SMPSEN) {
        return DAS_ERROR_UNSUPPORTED;
    }

    return wait_mask(&PWR->CSR1,
                     PWR_CSR1_ACTVOSRDY,
                     true,
                     wait_limit);
#else
#error "STM32H755 power backend requires CORE_CM7 or CORE_CM4"
#endif
}
