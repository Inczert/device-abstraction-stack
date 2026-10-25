// SPDX-License-Identifier: Apache-2.0
#ifndef DAS_WATCHDOG_H
#define DAS_WATCHDOG_H

#include <stdint.h>
#include <das/result.h>

/* Independent hardware watchdog; the MCU/backend selects its own instance.
 * STM32H755 CM7 uses IWDG1, CM4 uses IWDG2.
 *
 * The timeout is approximate because LSI is an uncalibrated oscillator:
 * lsi_hz is the application's measured/estimated frequency, not a guarantee.
 * The caller supplies a finite polling bound in case register updates stall.
 */
typedef struct das_watchdog_config {
    uint32_t timeout_ms;
    uint32_t lsi_hz;
    uint32_t update_poll_limit;
} das_watchdog_config_t;

/** Start/configure IWDG; cannot be stopped until hardware reset. Config
 * validation completes BEFORE any hardware start. A timeout after starting
 * means the watchdog may still be active; the caller must not ignore it.
 * The optional actual_timeout_ms reports the rounded nominal interval.
 */
das_result_t das_watchdog_start(const das_watchdog_config_t* config,
                                uint32_t* actual_timeout_ms);

/** Refresh the current core's independent watchdog. No task supervision or
 * scheduler policy is implied. Also usable after a watchdog-triggered reboot,
 * or when hardware option bytes started the watchdog automatically.
 */
das_result_t das_watchdog_feed(void);

#endif
