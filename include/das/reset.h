// SPDX-License-Identifier: Apache-2.0
#ifndef DAS_RESET_H
#define DAS_RESET_H

#include <stdint.h>
#include <das/result.h>

/* Flags are a set of potentially simultaneous hardware reset causes.
 * In particular a watchdog-triggered system reset may ALSO assert PIN.
 * Never infer a single exclusive reason from one bit.
 */
#define DAS_RESET_POWER_ON      (UINT32_C(1) << 0)
#define DAS_RESET_BROWN_OUT     (UINT32_C(1) << 1)
#define DAS_RESET_EXTERNAL_PIN  (UINT32_C(1) << 2)
#define DAS_RESET_SOFTWARE      (UINT32_C(1) << 3)
#define DAS_RESET_IWDG1         (UINT32_C(1) << 4)
#define DAS_RESET_IWDG2         (UINT32_C(1) << 5)
#define DAS_RESET_WWDG1         (UINT32_C(1) << 6)
#define DAS_RESET_WWDG2         (UINT32_C(1) << 7)
#define DAS_RESET_LOW_POWER     (UINT32_C(1) << 8)
#define DAS_RESET_CORE         (UINT32_C(1) << 9)
#define DAS_RESET_DOMAIN       (UINT32_C(1) << 10)

typedef struct das_reset_cause {
    uint32_t flags;
    /** Raw backend-specific flags, for diagnostics, NOT a portable ABI. */
    uint32_t raw_status;
} das_reset_cause_t;

/** Read latched reset status. Never implicitly clear it.
 * On dual-core STM32H755 the RCC reset flags are SHARED between both cores.
 * The application must appoint one startup owner to snapshot the cause
 * before either core calls das_reset_cause_clear().
 */
das_result_t das_reset_cause_get(das_reset_cause_t* cause);

/** Explicitly clear shared latched reset flags. Do not call automatically. */
das_result_t das_reset_cause_clear(void);

#endif
