// SPDX-License-Identifier: Apache-2.0
#ifndef DAS_CORE_H
#define DAS_CORE_H

#include <stdbool.h>
#include <das/result.h>

/**
 * Hardware-observable secondary-core boot state, not execution confirmation.
 * A clock-ready domain is NOT proof that CM4 entered its application's main().
 */
typedef struct das_secondary_core_state {
    bool boot_requested;
    bool clock_domain_ready;
} das_secondary_core_state_t;

/** Request a previously gated secondary core to boot. STM32H755: CM7-only. 
 * Does not reflash, reset, or remotely stop CM4; the CM4 boot image and its
 * device boot-address configuration must already be valid.
 * Returns UNSUPPORTED from a CM4 build. May be called more than once.
 */
das_result_t das_secondary_core_request_boot(void);

/** Read boot-request and clock-domain flags. Does not assert CM4 is running. */
das_result_t das_secondary_core_get_state(das_secondary_core_state_t* state);

#endif
