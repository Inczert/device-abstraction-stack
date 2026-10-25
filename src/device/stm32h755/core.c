// SPDX-License-Identifier: Apache-2.0
#include <das/core.h>
#include "stm32h755xx.h"

/* The RCC boot gate is hardware capability, not an application heartbeat. */
das_result_t das_secondary_core_request_boot(void) {
#if defined(CORE_CM7)
    RCC->GCR |= RCC_GCR_BOOT_C2;
    __DSB();
    return (RCC->GCR & RCC_GCR_BOOT_C2) != 0u ? DAS_OK : DAS_ERROR_IO;
#elif defined(CORE_CM4)
    return DAS_ERROR_UNSUPPORTED;
#else
#error "STM32H755 CM7 or CM4 required"
#endif
}

das_result_t das_secondary_core_get_state(das_secondary_core_state_t* state) {
    if (state == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    state->boot_requested = (RCC->GCR & RCC_GCR_BOOT_C2) != 0u;
    state->clock_domain_ready = (RCC->CR & RCC_CR_D2CKRDY) != 0u;
    return DAS_OK;
}
