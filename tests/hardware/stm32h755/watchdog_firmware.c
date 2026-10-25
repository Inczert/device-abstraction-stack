// SPDX-License-Identifier: Apache-2.0
/*
 * CM7: two genuine resets in one physical campaign case:
 *   initial boot -> software reset -> verify SOFT cause, clear;
 *   second boot -> IWDG timeout -> verify IWDG cause, clear.
 *
 * GDB sets both .noinit markers to 0 before the first run. Their values
 * survive system resets but are never initialized automatically by startup.
 * .bss evidence is reinitialized on EACH boot; only the final boot passes.
 */
#include <das/reset.h>
#include <das/watchdog.h>
#include "stm32h755xx.h"
#include <stdint.h>
#include <stddef.h>

#if defined(CORE_CM7)
#define WD_EXPECTED_FLAG DAS_RESET_IWDG1
#define WD_HW IWDG1
#define WD_DBG_REG DBGMCU->APB4FZ1
#define WD_DBG_FREEZE DBGMCU_APB4FZ1_DBG_IWDG1
#elif defined(CORE_CM4)
#define WD_EXPECTED_FLAG DAS_RESET_IWDG2
#define WD_HW IWDG2
#define WD_DBG_REG DBGMCU->APB4FZ2
#define WD_DBG_FREEZE DBGMCU_APB4FZ2_DBG_IWDG2
#else
#error "STM32H755 watchdog fixture requires CORE_CM7 or CORE_CM4"
#endif

#define WD_MAGIC UINT32_C(0x44535744)
#define WD_SOFTWARE_STAGE UINT32_C(0x534F4654)
#define WD_CONFIGURING_STAGE UINT32_C(0x434F4E46)
#define WD_ARMED_STAGE UINT32_C(0x57444731)
#define WD_TIMEOUT_MS UINT32_C(1200)

volatile uint32_t g_das_watchdog_stage __attribute__((section(".noinit")));
volatile uint32_t g_das_watchdog_saved_software __attribute__((section(".noinit")));
volatile uint32_t g_das_watchdog_boot_count __attribute__((section(".noinit")));
volatile uint32_t g_das_watchdog_option_status __attribute__((section(".noinit")));
volatile uint32_t g_das_watchdog_hw_start __attribute__((section(".noinit")));
/* Persistent evidence even if reset happens during watchdog programming. */
volatile uint32_t g_das_wd_dbg_before __attribute__((section(".noinit")));
volatile uint32_t g_das_wd_dbg_after __attribute__((section(".noinit")));
volatile uint32_t g_das_wd_clock_csr __attribute__((section(".noinit")));
volatile uint32_t g_das_wd_pr __attribute__((section(".noinit")));
volatile uint32_t g_das_wd_rlr __attribute__((section(".noinit")));
volatile uint32_t g_das_wd_sr __attribute__((section(".noinit")));
/* Durable checkpoints distinguish a reset in start() from a reset during
 * test-side diagnostic register reads. They are not part of the DAS API.
 */
volatile int32_t g_das_wd_start_return __attribute__((section(".noinit")));
volatile uint32_t g_das_wd_start_actual __attribute__((section(".noinit")));
volatile uint32_t g_das_wd_checkpoint __attribute__((section(".noinit")));

typedef struct {
    uint32_t magic;
    volatile uint32_t complete;
    volatile uint32_t error;
    volatile int32_t start_result;
    volatile uint32_t actual_ms;
    volatile uint32_t software_flags;
    volatile uint32_t reset_flags;
    volatile uint32_t raw_rsr;
    volatile uint32_t cleared_flags;
    volatile uint32_t heartbeat;
} wd_evidence_t;

volatile wd_evidence_t g_das_watchdog_evidence = {
    .magic = WD_MAGIC, .start_result = DAS_ERROR_NOT_READY
};

static void fail(uint32_t code) {
    g_das_watchdog_evidence.error = code;
    for (;;) { (void)das_watchdog_feed(); }
}

int main(void) {
    /* Feed at the earliest application entry: option bytes can enable IWDG
     * automatically. This must never be mistaken for a DAS software start.
     * The ARMED stage intentionally stops feeding only AFTER a successful
     * software configuration.
     */
    (void)das_watchdog_feed();
    ++g_das_watchdog_boot_count;
    /*
     * An attached OpenOCD/debugger can arrange for IWDG to stop while a
     * core is halted. That would invalidate a physical timeout qualifier.
     * Disable watchdog debug freeze for THIS destructive test only, not
     * in the production DAS driver. Never modify option bytes.
     */
    g_das_wd_dbg_before = WD_DBG_REG;
    WD_DBG_REG &= ~WD_DBG_FREEZE;
    __DSB();
    g_das_wd_dbg_after = WD_DBG_REG;
    g_das_watchdog_option_status = FLASH->OPTSR_CUR;
#if defined(CORE_CM7) && defined(FLASH_OPTSR_IWDG1_SW)
    g_das_watchdog_hw_start =
        (FLASH->OPTSR_CUR & FLASH_OPTSR_IWDG1_SW) == 0u ? 1u : 0u;
#elif defined(CORE_CM4) && defined(FLASH_OPTSR_IWDG2_SW)
    g_das_watchdog_hw_start =
        (FLASH->OPTSR_CUR & FLASH_OPTSR_IWDG2_SW) == 0u ? 1u : 0u;
#else
    g_das_watchdog_hw_start = UINT32_MAX; /* unknown mapping */
#endif
    __DMB();
    das_reset_cause_t cause = {0};
    if (das_reset_cause_get(NULL) != DAS_ERROR_INVALID_ARGUMENT ||
        das_reset_cause_get(&cause) != DAS_OK) {
        fail(1u);
    }

    if (g_das_watchdog_stage == WD_ARMED_STAGE) {
        /* Report raw RCC flags even when an expected bit is missing. */
        g_das_watchdog_evidence.reset_flags = cause.flags;
        g_das_watchdog_evidence.raw_rsr = cause.raw_status;
        if ((cause.flags & WD_EXPECTED_FLAG) == 0u ||
            (cause.flags & DAS_RESET_SOFTWARE) != 0u ||
            (g_das_watchdog_saved_software & DAS_RESET_SOFTWARE) == 0u ||
            (g_das_watchdog_saved_software & WD_EXPECTED_FLAG) != 0u) {
            fail(2u);
        }

        g_das_watchdog_stage = 0u;
        g_das_watchdog_evidence.start_result = DAS_OK;
        g_das_watchdog_evidence.actual_ms = WD_TIMEOUT_MS;
        g_das_watchdog_evidence.software_flags = g_das_watchdog_saved_software;
        g_das_watchdog_evidence.reset_flags = cause.flags;
        g_das_watchdog_evidence.raw_rsr = cause.raw_status;

        if (das_reset_cause_clear() != DAS_OK ||
            das_reset_cause_get(&cause) != DAS_OK ||
            (cause.flags & (WD_EXPECTED_FLAG | DAS_RESET_SOFTWARE)) != 0u) {
            fail(3u);
        }
        g_das_watchdog_evidence.cleared_flags = cause.flags;
        g_das_watchdog_evidence.complete = 1u;
        for (;;) {
            (void)das_watchdog_feed();
            ++g_das_watchdog_evidence.heartbeat;
        }
    }

    if (g_das_watchdog_stage == WD_CONFIGURING_STAGE) {
        /* A reset during watchdog_start() must never count as a successful
         * IWDG timeout test: the API had not returned DAS_OK yet.
         */
        g_das_watchdog_evidence.reset_flags = cause.flags;
        g_das_watchdog_evidence.raw_rsr = cause.raw_status;
        fail(9u);
    }

    if (g_das_watchdog_stage == WD_SOFTWARE_STAGE) {
        /* Record the actual observed status BEFORE the assertions, so a
         * failure can distinguish a missing RCC flag from wrong mapping.
         */
        g_das_watchdog_evidence.software_flags = cause.flags;
        g_das_watchdog_evidence.raw_rsr = cause.raw_status;
        if ((cause.flags & DAS_RESET_SOFTWARE) == 0u ||
            (cause.flags & WD_EXPECTED_FLAG) != 0u) {
            fail(4u);
        }
        g_das_watchdog_saved_software = cause.flags;

        /* The second phase must prove explicit clearing, before arming IWDG. */
        if (das_reset_cause_clear() != DAS_OK ||
            das_reset_cause_get(&cause) != DAS_OK ||
            (cause.flags & (DAS_RESET_SOFTWARE | WD_EXPECTED_FLAG)) != 0u) {
            fail(5u);
        }

        const das_watchdog_config_t invalid = {0};
        if (das_watchdog_start(&invalid, NULL) != DAS_ERROR_INVALID_ARGUMENT ||
            das_watchdog_start(NULL, NULL) != DAS_ERROR_INVALID_ARGUMENT) {
            fail(6u);
        }
        const das_watchdog_config_t config = {
            .timeout_ms = WD_TIMEOUT_MS,
            .lsi_hz = UINT32_C(32000),
            .update_poll_limit = UINT32_C(1000000)
        };
        /* New persistent phase separates "unexpected watchdog before
         * configuring" from "watchdog reset during configuration".
         */
        g_das_wd_checkpoint = 0u;
        g_das_wd_start_return = DAS_ERROR_NOT_READY;
        g_das_wd_start_actual = 0u;
        g_das_watchdog_stage = WD_CONFIGURING_STAGE;
        __DMB();
        uint32_t actual = 0u;
        const das_result_t started = das_watchdog_start(&config, &actual);

        /* Capture the return before any register diagnostics. They must not
         * delay the crucial handoff into the intentional no-feed interval.
         */
        g_das_wd_start_return = started;
        g_das_wd_start_actual = actual;
        g_das_wd_checkpoint = 1u;
        if (started != DAS_OK || actual != WD_TIMEOUT_MS) {
            /* A failed start may still leave the watchdog running. Never
             * accept its reset as a qualified deliberate timeout.
             */
            g_das_watchdog_stage = 0u;
            fail(7u);
        }

        /* The start API has completed successfully. Explicitly refresh the
         * counter to give the test the entire configured timeout, THEN arm
         * the persisted success marker. No IWDG feeds occur after arming
         * until the hardware timeout produces the next system reset.
         */
        (void)das_watchdog_feed();
        g_das_wd_checkpoint = 2u;
        g_das_watchdog_stage = WD_ARMED_STAGE;
        __DSB();

        /* Capture optional diagnostics only after successful handoff. */
        g_das_wd_clock_csr = RCC->CSR;
        g_das_wd_pr = WD_HW->PR;
        g_das_wd_rlr = WD_HW->RLR;
        g_das_wd_sr = WD_HW->SR;
        g_das_wd_checkpoint = 3u;
        for (;;) { __NOP(); } /* Deliberately do not feed. */
    }

    /* First boot: throw away any stale reset reason from previous campaigns.
     * One controlled NVIC software reset will establish a distinct baseline.
     */
    if (das_reset_cause_clear() != DAS_OK ||
        das_reset_cause_get(&cause) != DAS_OK ||
        (cause.flags & (DAS_RESET_SOFTWARE | WD_EXPECTED_FLAG)) != 0u) {
        fail(8u);
    }
    g_das_watchdog_saved_software = 0u;
    g_das_watchdog_stage = WD_SOFTWARE_STAGE;
    __DMB();
    NVIC_SystemReset();
    for (;;) { __NOP(); } /* Unreachable if CPU reset worked. */
}

void HardFault_Handler(void) { fail(UINT32_C(0xdead)); }
