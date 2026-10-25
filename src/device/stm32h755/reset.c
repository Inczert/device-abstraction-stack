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
    if ((rsr & RCC_RSR_SFTRSTF) != 0u) flags |= DAS_RESET_SOFTWARE;
    if ((rsr & RCC_RSR_IWDG1RSTF) != 0u) flags |= DAS_RESET_IWDG1;
    if ((rsr & RCC_RSR_IWDG2RSTF) != 0u) flags |= DAS_RESET_IWDG2;
    if ((rsr & RCC_RSR_WWDG1RSTF) != 0u) flags |= DAS_RESET_WWDG1;
    if ((rsr & RCC_RSR_WWDG2RSTF) != 0u) flags |= DAS_RESET_WWDG2;
    if ((rsr & RCC_RSR_LPWRRSTF) != 0u) flags |= DAS_RESET_LOW_POWER;
    if ((rsr & (RCC_RSR_CPURSTF | RCC_RSR_C2RSTF)) != 0u)
        flags |= DAS_RESET_CORE;
    if ((rsr & (RCC_RSR_D1RSTF | RCC_RSR_D2RSTF)) != 0u)
        flags |= DAS_RESET_DOMAIN;

    cause->flags = flags;
    cause->raw_status = rsr;
    return DAS_OK;
}

das_result_t das_reset_cause_clear(void) {
    RCC->RSR |= RCC_RSR_RMVF;
    __DSB();
    return DAS_OK;
}
