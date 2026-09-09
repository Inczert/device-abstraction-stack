// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_IRQ_H
#define DAS_IRQ_H

#include <stdbool.h>
#include <stdint.h>

#include <das/result.h>

/**
 * Device-agnostic interrupt-controller line handle.
 *
 * The storage is backend-owned. Applications must obtain handles from DAS
 * resource APIs rather than interpreting or constructing controller numbers.
 */
typedef struct das_irq {
    uint32_t storage;
} das_irq_t;

/** Invalid/uninitialized interrupt handle. */
#define DAS_IRQ_INVALID ((das_irq_t){UINT32_MAX})

/** Return whether a handle is valid for the selected interrupt backend. */
bool das_irq_is_valid(das_irq_t irq);

/**
 * Number of priority levels supported by the selected interrupt controller.
 *
 * Priority 0 is the highest priority. Valid values passed to
 * das_irq_set_priority() are 0 .. das_irq_priority_levels()-1.
 */
uint32_t das_irq_priority_levels(void);

/** Enable delivery of this interrupt-controller line to the selected CPU. */
das_result_t das_irq_enable(das_irq_t irq);

/** Disable delivery of this interrupt-controller line to the selected CPU. */
das_result_t das_irq_disable(das_irq_t irq);

/** Query whether this interrupt-controller line is enabled. */
das_result_t das_irq_is_enabled(das_irq_t irq, bool* enabled);

/** Set the controller priority. Priority 0 is highest. */
das_result_t das_irq_set_priority(das_irq_t irq, uint32_t priority);

/** Read the current controller priority. */
das_result_t das_irq_get_priority(das_irq_t irq, uint32_t* priority);

/** Set this interrupt-controller line pending in software. */
das_result_t das_irq_set_pending(das_irq_t irq);

/** Clear controller pending state for this interrupt line. */
das_result_t das_irq_clear_pending(das_irq_t irq);

/** Query controller pending state for this interrupt line. */
das_result_t das_irq_is_pending(das_irq_t irq, bool* pending);

#endif
