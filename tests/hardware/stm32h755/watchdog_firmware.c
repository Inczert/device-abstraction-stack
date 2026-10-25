// SPDX-License-Identifier: Apache-2.0
/* CM7: deliberately fail to feed IWDG1, reboot, verify sticky RCC status.
 * The GDB campaign initializes g_das_watchdog_stage once, before running.
 * .noinit state persists over system reset; .bss evidence is reconstructed
 * by the firmware AFTER the watchdog reset.
 */
#include <das/reset.h>
#include <das/watchdog.h>
#include "stm32h755xx.h"
#include <stdint.h>
#include <stddef.h>

#if defined(CORE_CM7)
#define WD_EXPECTED_FLAG DAS_RESET_IWDG1
#elif defined(CORE_CM4)
#define WD_EXPECTED_FLAG DAS_RESET_IWDG2
#else
#error "STM32H755 watchdog test requires CORE_CM7 or CORE_CM4"
#endif

#define WD_MAGIC UINT32_C(0x44535744)
#define WD_ARMED UINT32_C(0x57444731)
#define WD_TIMEOUT_MS UINT32_C(1200)

volatile uint32_t g_das_watchdog_stage __attribute__((section(".noinit")));

typedef struct {
    uint32_t magic;
    volatile uint32_t complete;
    volatile uint32_t error;
    volatile int32_t start_result;
    volatile uint32_t actual_ms;
    volatile uint32_t reset_flags;
    volatile uint32_t raw_rsr;
    volatile uint32_t cleared_flags;
    volatile uint32_t heartbeat;
} wd_evidence_t;

volatile wd_evidence_t g_das_watchdog_evidence = {
    .magic = WD_MAGIC,
    .start_result = DAS_ERROR_NOT_READY
};

static void fail(uint32_t error) {
    g_das_watchdog_evidence.error = error;
    for (;;) { (void)das_watchdog_feed(); }
}

int main(void) {
    das_reset_cause_t cause = {0};
    if (das_reset_cause_get(NULL) != DAS_ERROR_INVALID_ARGUMENT ||
        das_reset_cause_get(&cause) != DAS_OK) {
        fail(1u);
    }

    if (g_das_watchdog_stage == WD_ARMED) {
        /* This path is reachable only after reset, not when initially armed. */
        if ((cause.flags & WD_EXPECTED_FLAG) == 0u) fail(2u);
        g_das_watchdog_stage = 0u;
        g_das_watchdog_evidence.start_result = DAS_OK;
        g_das_watchdog_evidence.actual_ms = WD_TIMEOUT_MS;
        g_das_watchdog_evidence.reset_flags = cause.flags;
        g_das_watchdog_evidence.raw_rsr = cause.raw_status;
        if (das_reset_cause_clear() != DAS_OK ||
            das_reset_cause_get(&cause) != DAS_OK ||
            (cause.flags & WD_EXPECTED_FLAG) != 0u) fail(3u);
        g_das_watchdog_evidence.cleared_flags = cause.flags;
        g_das_watchdog_evidence.complete = 1u;
        for (;;) {
            (void)das_watchdog_feed();
            ++g_das_watchdog_evidence.heartbeat;
        }
    }

    /* Only the designated CM7/CM4 hardware image is run for this check.
     * Reset flags from all prior tests must not satisfy the IWDG criterion.
     */
    if (das_reset_cause_clear() != DAS_OK ||
        das_reset_cause_get(&cause) != DAS_OK ||
        (cause.flags & WD_EXPECTED_FLAG) != 0u) {
        fail(4u);
    }

    const das_watchdog_config_t invalid = {0};
    if (das_watchdog_start(&invalid, NULL) != DAS_ERROR_INVALID_ARGUMENT ||
        das_watchdog_start(NULL, NULL) != DAS_ERROR_INVALID_ARGUMENT) fail(5u);

    const das_watchdog_config_t config = {
        .timeout_ms = WD_TIMEOUT_MS,
        .lsi_hz = UINT32_C(32000),
        .update_poll_limit = UINT32_C(1000000)
    };
    uint32_t actual = 0u;
    g_das_watchdog_evidence.start_result = das_watchdog_start(&config, &actual);
    if (g_das_watchdog_evidence.start_result != DAS_OK ||
        actual < UINT32_C(1200) || actual > UINT32_C(1250)) {
        /* The watchdog might already be started even if start() timed out.
         * Failure is observable; do not arm the reset-success sentinel.
         */
        g_das_watchdog_stage = 0u;
        fail(6u);
    }
    g_das_watchdog_evidence.actual_ms = actual;
    /* Only a successfully configured watchdog may count as a passing
     * intentional reset; do not accept watchdog reset during config error.
     */
    g_das_watchdog_stage = WD_ARMED;
    __DMB();

    /* An actual hardware timeout MUST reset the MCU. If this loop continues,
     * GDB will fail the test instead of reporting success.
     */
    for (;;) { __NOP(); }
}

void HardFault_Handler(void) { fail(UINT32_C(0xdead)); }
