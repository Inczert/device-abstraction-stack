// SPDX-License-Identifier: Apache-2.0
#include <das/reset.h>
#include "stm32h755xx.h"

#include <stddef.h>
#include <stdint.h>

/*
 * RCC reset flags are SHARED by CPU1 and CPU2. This API has no hidden
 * startup hook and deliberately never clears flags while reading.
 * Applications coordinate a single boot-time snapshot/clear owner.
 */
das_result_t das_reset_cause_get(das_reset_cause_t* cause) {
    if (cause == NULL) return DAS_ERROR_INVALID_ARGUMENT;

    const uint32_t rsr = RCC->RSR;
    uint32_t flags = 0u;
    if ((rsr & RCC_RSR_PORRSTF) != 0u) flags |= DAS_RESET_POWER_ON;
    if ((rsr & RCC_RSR_BORRSTF) != 0u) flags |= DAS_RESET_BROWN_OUT;
    if ((rsr & RCC_RSR_PINRSTF) != 0u) flags |= DAS_RESET_EXTERNAL_PIN;
    /* Dual-core H755 CMSIS has separate CPU1/CPU2 flags, unlike the
     * single-core STM32H7 RCC register definition.
     */
#if defined(RCC_RSR_SFT1RSTF)
    if ((rsr & RCC_RSR_SFT1RSTF) != 0u) flags |= DAS_RESET_SOFTWARE;
#endif
#if defined(RCC_RSR_SFT2RSTF)
    if ((rsr & RCC_RSR_SFT2RSTF) != 0u) flags |= DAS_RESET_SOFTWARE;
#endif
    if ((rsr & RCC_RSR_IWDG1RSTF) != 0u) flags |= DAS_RESET_IWDG1;
    if ((rsr & RCC_RSR_IWDG2RSTF) != 0u) flags |= DAS_RESET_IWDG2;
    if ((rsr & RCC_RSR_WWDG1RSTF) != 0u) flags |= DAS_RESET_WWDG1;
    if ((rsr & RCC_RSR_WWDG2RSTF) != 0u) flags |= DAS_RESET_WWDG2;
#if defined(RCC_RSR_LPWR1RSTF)
    if ((rsr & RCC_RSR_LPWR1RSTF) != 0u) flags |= DAS_RESET_LOW_POWER;
#endif
#if defined(RCC_RSR_LPWR2RSTF)
    if ((rsr & RCC_RSR_LPWR2RSTF) != 0u) flags |= DAS_RESET_LOW_POWER;
#endif
#if defined(RCC_RSR_C1RSTF)
    if ((rsr & RCC_RSR_C1RSTF) != 0u) flags |= DAS_RESET_CORE;
#endif
#if defined(RCC_RSR_C2RSTF)
    if ((rsr & RCC_RSR_C2RSTF) != 0u) flags |= DAS_RESET_CORE;
#endif
    if ((rsr & (RCC_RSR_D1RSTF | RCC_RSR_D2RSTF)) != 0u)
        flags |= DAS_RESET_DOMAIN;

    cause->flags = flags;
    cause->raw_status = rsr;
    return DAS_OK;
}

das_result_t das_reset_cause_clear(void) {
    /*
     * RMVF is a software-controlled reset-flags CLEAR input, not an
     * application-owned sticky status bit. Pulse it and then explicitly
     * deassert it: leaving RMVF asserted can suppress latching the cause
     * of a subsequent reset. The other RSR bits are read-only status.
     * One core/application must own the clear operation.
     */
    RCC->RSR = RCC_RSR_RMVF;
    __DSB();
    (void)RCC->RSR;
    RCC->RSR = 0u;
    __DSB();
    (void)RCC->RSR;
    return DAS_OK;
}
