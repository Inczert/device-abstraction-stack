// SPDX-License-Identifier: Apache-2.0
#include <das/hsem.h>
#include "stm32h755xx.h"

#if defined(CORE_CM7)
#define DAS_HSEM_CORE_ID HSEM_CPU1_COREID
#define DAS_HSEM_OWN_IER (HSEM->C1IER)
#define DAS_HSEM_OWN_MISR (HSEM->C1MISR)
#define DAS_HSEM_OWN_ICR (HSEM->C1ICR)
#define DAS_HSEM_IRQ HSEM1_IRQn
#elif defined(CORE_CM4)
#define DAS_HSEM_CORE_ID HSEM_CPU2_COREID
#define DAS_HSEM_OWN_IER (HSEM->C2IER)
#define DAS_HSEM_OWN_MISR (HSEM->C2MISR)
#define DAS_HSEM_OWN_ICR (HSEM->C2ICR)
#define DAS_HSEM_IRQ HSEM2_IRQn
#else
#error "STM32H755 CM7 or CM4 required"
#endif

#define DAS_HSEM_SELF_OWNER ((uint32_t)(DAS_HSEM_CORE_ID) << HSEM_R_COREID_Pos)

static bool valid(uint32_t channel) {
    return channel < DAS_HSEM_CHANNEL_COUNT;
}

static bool clock_enabled(void) {
    return (RCC->AHB4ENR & RCC_AHB4ENR_HSEMEN) != 0u;
}

das_result_t das_hsem_init(void) {
    RCC->AHB4ENR |= RCC_AHB4ENR_HSEMEN;
    (void)RCC->AHB4ENR;
    __DSB();
    return clock_enabled() ? DAS_OK : DAS_ERROR_IO;
}

das_result_t das_hsem_try_acquire(uint32_t channel) {
    if (!valid(channel)) return DAS_ERROR_INVALID_ARGUMENT;
    if (!clock_enabled()) return DAS_ERROR_NOT_READY;
    /* RLR read atomically attempts acquisition; only own core and PID 0 win. */
    const uint32_t result = HSEM->RLR[channel];
    return result == (HSEM_RLR_LOCK | DAS_HSEM_SELF_OWNER)
           ? DAS_OK : DAS_ERROR_NOT_READY;
}

das_result_t das_hsem_release(uint32_t channel) {
    if (!valid(channel)) return DAS_ERROR_INVALID_ARGUMENT;
    if (!clock_enabled()) return DAS_ERROR_NOT_READY;
    const uint32_t value = HSEM->R[channel];
    if (value != (HSEM_R_LOCK | DAS_HSEM_SELF_OWNER)) {
        return DAS_ERROR_NOT_READY;
    }
    __DMB();
    HSEM->R[channel] = DAS_HSEM_SELF_OWNER;
    __DMB();
    return (HSEM->R[channel] & HSEM_R_LOCK) == 0u ? DAS_OK : DAS_ERROR_IO;
}

das_result_t das_hsem_is_locked(uint32_t channel, bool* locked) {
    if (!valid(channel) || locked == 0) return DAS_ERROR_INVALID_ARGUMENT;
    if (!clock_enabled()) return DAS_ERROR_NOT_READY;
    *locked = (HSEM->R[channel] & HSEM_R_LOCK) != 0u;
    return DAS_OK;
}

das_result_t das_hsem_set_notification(uint32_t channel, bool enable) {
    if (!valid(channel)) return DAS_ERROR_INVALID_ARGUMENT;
    if (!clock_enabled()) return DAS_ERROR_NOT_READY;
    const uint32_t old_primask = __get_PRIMASK();
    __disable_irq();
    if (enable) {
        DAS_HSEM_OWN_IER |= UINT32_C(1) << channel;
    } else {
        DAS_HSEM_OWN_IER &= ~(UINT32_C(1) << channel);
    }
    __set_PRIMASK(old_primask);
    return DAS_OK;
}

das_result_t das_hsem_notification_pending(uint32_t channel, bool* pending) {
    if (!valid(channel) || pending == 0) return DAS_ERROR_INVALID_ARGUMENT;
    if (!clock_enabled()) return DAS_ERROR_NOT_READY;
    *pending = (DAS_HSEM_OWN_MISR & (UINT32_C(1) << channel)) != 0u;
    return DAS_OK;
}

das_result_t das_hsem_clear_notification(uint32_t channel) {
    if (!valid(channel)) return DAS_ERROR_INVALID_ARGUMENT;
    if (!clock_enabled()) return DAS_ERROR_NOT_READY;
    DAS_HSEM_OWN_ICR = UINT32_C(1) << channel;
    __DSB();
    return DAS_OK;
}

das_result_t das_hsem_get_irq(das_irq_t* irq) {
    if (irq == 0) return DAS_ERROR_INVALID_ARGUMENT;
    *irq = (das_irq_t){.storage = (uint32_t)DAS_HSEM_IRQ};
    return DAS_OK;
}
