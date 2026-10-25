// SPDX-License-Identifier: Apache-2.0
#include <das/watchdog.h>

#include "stm32h755xx.h"

#include <stddef.h>
#include <stdint.h>

#if defined(CORE_CM7)
#define DAS_OWN_IWDG IWDG1
#elif defined(CORE_CM4)
#define DAS_OWN_IWDG IWDG2
#else
#error "Watchdog requires CORE_CM7 or CORE_CM4"
#endif

#define DAS_WDG_START_KEY UINT32_C(0xCCCC)
#define DAS_WDG_UNLOCK_KEY UINT32_C(0x5555)
#define DAS_WDG_FEED_KEY UINT32_C(0xAAAA)
#define DAS_WDG_RELOAD_MAX UINT32_C(0xFFF)

/* PR encoding 0..6 corresponds to /4,/8,/16,/32,/64,/128,/256. */
static const uint16_t g_dividers[] = {4u, 8u, 16u, 32u, 64u, 128u, 256u};

das_result_t das_watchdog_start(const das_watchdog_config_t* config,
                                uint32_t* actual_timeout_ms) {
    if (config == NULL || config->timeout_ms == 0u ||
        config->lsi_hz < UINT32_C(10000) ||
        config->lsi_hz > UINT32_C(100000) ||
        config->update_poll_limit == 0u) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    /* Calculate first, before the irreversible peripheral start.
     * 64-bit intermediates prevent overflow for untrusted timeouts.
     */
    uint32_t prescaler = UINT32_MAX;
    uint32_t reload = 0u;
    uint32_t divisor = 0u;
    for (uint32_t i = 0u; i < sizeof(g_dividers)/sizeof(g_dividers[0]); ++i) {
        const uint32_t div = g_dividers[i];
        const uint64_t cycles = ((uint64_t)config->timeout_ms * config->lsi_hz +
                                 (uint64_t)1000u * div - 1u) /
                                ((uint64_t)1000u * div);
        if (cycles >= 2u && cycles <= DAS_WDG_RELOAD_MAX + UINT64_C(1)) {
            prescaler = i;
            reload = (uint32_t)cycles - 1u;
            divisor = div;
            break;
        }
    }
    if (prescaler == UINT32_MAX) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    const uint32_t actual = (uint32_t)(
        (((uint64_t)reload + 1u) * divisor * 1000u + config->lsi_hz - 1u) /
        config->lsi_hz);

    /* STM32 IWDG start is permanent until reset; no HAL or global clock
     * reconfiguration. Timeout here does NOT mean start was rolled back.
     */
    DAS_OWN_IWDG->KR = DAS_WDG_START_KEY;
    DAS_OWN_IWDG->KR = DAS_WDG_UNLOCK_KEY;
    DAS_OWN_IWDG->PR = prescaler;
    DAS_OWN_IWDG->RLR = reload;
    DAS_OWN_IWDG->WINR = DAS_WDG_RELOAD_MAX; /* Disable window restriction. */
    for (uint32_t i = 0u; i < config->update_poll_limit; ++i) {
        if ((DAS_OWN_IWDG->SR &
             (IWDG_SR_PVU | IWDG_SR_RVU | IWDG_SR_WVU)) == 0u) {
            DAS_OWN_IWDG->KR = DAS_WDG_FEED_KEY;
            if (actual_timeout_ms != NULL) {
                *actual_timeout_ms = actual;
            }
            return DAS_OK;
        }
    }
    /* Best effort: keep active watchdog fed even though update timed out. */
    DAS_OWN_IWDG->KR = DAS_WDG_FEED_KEY;
    return DAS_ERROR_TIMEOUT;
}

das_result_t das_watchdog_feed(void) {
    DAS_OWN_IWDG->KR = DAS_WDG_FEED_KEY;
    return DAS_OK;
}
