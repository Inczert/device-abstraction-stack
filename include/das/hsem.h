// SPDX-License-Identifier: Apache-2.0
#ifndef DAS_HSEM_H
#define DAS_HSEM_H

#include <stdbool.h>
#include <stdint.h>
#include <das/irq.h>
#include <das/result.h>

/**
 * Hardware semaphore channels are selected by caller agreement (0..31 on
 * STM32H755). They are global across both cores; DAS reserves none.
 * Fast-take uses hardware process ID 0 and is NOT recursive. Applications
 * must coordinate ownership and release; no RTOS policy or blocking waits.
 */
#define DAS_HSEM_CHANNEL_COUNT 32u

/** Enable HSEM's peripheral clock for the executing core. Safe to repeat. */
das_result_t das_hsem_init(void);

/** Attempt one atomic take. DAS_OK when acquired, NOT_READY when busy. */
das_result_t das_hsem_try_acquire(uint32_t channel);

/** Release only if owned by this core with process ID zero. */
das_result_t das_hsem_release(uint32_t channel);

/** Inspect whether a channel is presently held (regardless of owner). */
das_result_t das_hsem_is_locked(uint32_t channel, bool* locked);

/** Enable/disable release-event notification in this core's HSEM mask. */
das_result_t das_hsem_set_notification(uint32_t channel, bool enable);

/** Test whether an enabled release notification is pending for this core. */
das_result_t das_hsem_notification_pending(uint32_t channel, bool* pending);

/** Clear this core's notification latch (does not free/lock the channel). */
das_result_t das_hsem_clear_notification(uint32_t channel);

/** Get the interrupt-controller line; caller/RTOS owns NVIC/vector policy. */
das_result_t das_hsem_get_irq(das_irq_t* irq);

#endif
