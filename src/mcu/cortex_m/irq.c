// SPDX-License-Identifier: Apache-2.0

#include <das/irq.h>

#ifndef DAS_CMSIS_DEVICE_HEADER
#error "Cortex-M IRQ backend requires a CMSIS device header"
#endif

/*
 * CMSIS-Core's NVIC helpers intentionally use the device-defined IRQn_Type and
 * __NVIC_PRIO_BITS. The selected target supplies that CMSIS device header at
 * build time; this source contains no vendor interrupt numbers or peripherals.
 */
#include DAS_CMSIS_DEVICE_HEADER

static bool irq_valid(das_irq_t irq) {
    const uint32_t line_count =
        (uint32_t)(sizeof(NVIC->IP) / sizeof(NVIC->IP[0]));
    return irq.storage < line_count;
}

static IRQn_Type native_irq(das_irq_t irq) {
    return (IRQn_Type)(int32_t)irq.storage;
}

bool das_irq_is_valid(das_irq_t irq) {
    return irq_valid(irq);
}

uint32_t das_irq_priority_levels(void) {
    return UINT32_C(1) << __NVIC_PRIO_BITS;
}

das_result_t das_irq_enable(das_irq_t irq) {
    if (!irq_valid(irq)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    NVIC_EnableIRQ(native_irq(irq));
    return DAS_OK;
}

das_result_t das_irq_disable(das_irq_t irq) {
    if (!irq_valid(irq)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    NVIC_DisableIRQ(native_irq(irq));
    return DAS_OK;
}

das_result_t das_irq_is_enabled(das_irq_t irq, bool* enabled) {
    if (!irq_valid(irq) || enabled == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    *enabled = NVIC_GetEnableIRQ(native_irq(irq)) != 0u;
    return DAS_OK;
}

das_result_t das_irq_set_priority(das_irq_t irq, uint32_t priority) {
    if (!irq_valid(irq) || priority >= das_irq_priority_levels()) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    NVIC_SetPriority(native_irq(irq), priority);
    return DAS_OK;
}

das_result_t das_irq_get_priority(das_irq_t irq, uint32_t* priority) {
    if (!irq_valid(irq) || priority == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    *priority = NVIC_GetPriority(native_irq(irq));
    return DAS_OK;
}

das_result_t das_irq_set_pending(das_irq_t irq) {
    if (!irq_valid(irq)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    NVIC_SetPendingIRQ(native_irq(irq));
    return DAS_OK;
}

das_result_t das_irq_clear_pending(das_irq_t irq) {
    if (!irq_valid(irq)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    NVIC_ClearPendingIRQ(native_irq(irq));
    return DAS_OK;
}

das_result_t das_irq_is_pending(das_irq_t irq, bool* pending) {
    if (!irq_valid(irq) || pending == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    *pending = NVIC_GetPendingIRQ(native_irq(irq)) != 0u;
    return DAS_OK;
}
