// SPDX-License-Identifier: Apache-2.0
#include <das/core.h>
#include <das/hsem.h>
#include <das/result.h>
#include "stm32h755xx.h"
#include <stdint.h>
#include <stdbool.h>

#define MAGIC UINT32_C(0x44534853)
#define CHANNEL UINT32_C(29)

typedef struct {
    uint32_t magic;
    volatile uint32_t complete;
    volatile uint32_t error;
    volatile uint32_t flags;
    volatile int32_t request_result;
    volatile uint32_t heartbeat;
} evidence_t;

volatile evidence_t g_das_hsem_evidence = {
    .magic = MAGIC,
    .request_result = DAS_ERROR_NOT_READY,
};

static void fail(uint32_t code) {
    g_das_hsem_evidence.error = code;
    for (;;) { __NOP(); }
}

int main(void) {
    das_secondary_core_state_t state = {0};
    if (das_secondary_core_get_state(&state) != DAS_OK ||
        das_secondary_core_get_state(0) != DAS_ERROR_INVALID_ARGUMENT) {
        fail(1u);
    }
    g_das_hsem_evidence.flags |= 1u;

#if defined(CORE_CM4)
    g_das_hsem_evidence.request_result = das_secondary_core_request_boot();
    if (g_das_hsem_evidence.request_result != DAS_ERROR_UNSUPPORTED) fail(2u);
#else
    /*
     * Avoid booting a second core unexpectedly inside a standalone HSEM
     * test: the real autonomous-boot fixture will exercise request_boot().
     */
    g_das_hsem_evidence.request_result = DAS_OK;
#endif
    if (das_hsem_init() != DAS_OK ||
        das_hsem_try_acquire(32u) != DAS_ERROR_INVALID_ARGUMENT ||
        das_hsem_release(32u) != DAS_ERROR_INVALID_ARGUMENT) fail(3u);
    g_das_hsem_evidence.flags |= 2u;

    bool locked = false;
    if (das_hsem_is_locked(CHANNEL, &locked) != DAS_OK || locked) fail(4u);
    if (das_hsem_try_acquire(CHANNEL) != DAS_OK) fail(5u);
    if (das_hsem_is_locked(CHANNEL, &locked) != DAS_OK || !locked) fail(6u);
    if (das_hsem_try_acquire(CHANNEL) != DAS_ERROR_NOT_READY) fail(7u);
    g_das_hsem_evidence.flags |= 4u;

    das_irq_t irq = DAS_IRQ_INVALID;
    if (das_hsem_get_irq(&irq) != DAS_OK || !das_irq_is_valid(irq)) fail(8u);

    /* A release event can be seen without enabling NVIC or an ISR. */
    if (das_hsem_clear_notification(CHANNEL) != DAS_OK ||
        das_hsem_set_notification(CHANNEL, true) != DAS_OK) fail(9u);
    if (das_hsem_release(CHANNEL) != DAS_OK) fail(10u);
    if (das_hsem_is_locked(CHANNEL, &locked) != DAS_OK || locked) fail(11u);
    if (das_hsem_release(CHANNEL) != DAS_ERROR_NOT_READY) fail(12u);
    bool pending = false;
    if (das_hsem_notification_pending(CHANNEL, &pending) != DAS_OK || !pending) fail(13u);
    if (das_hsem_clear_notification(CHANNEL) != DAS_OK ||
        das_hsem_notification_pending(CHANNEL, &pending) != DAS_OK || pending ||
        das_hsem_set_notification(CHANNEL, false) != DAS_OK) fail(14u);
    g_das_hsem_evidence.flags |= 8u;

    g_das_hsem_evidence.complete = 1u;
    for (;;) { ++g_das_hsem_evidence.heartbeat; }
}

void HardFault_Handler(void) { fail(UINT32_C(0xdead)); }
