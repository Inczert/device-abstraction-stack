// SPDX-License-Identifier: Apache-2.0
/*
 * Autonomous dual-core hardware fixture. The caller only resumes CM7;
 * a valid CM4 image in flash bank 2 must respond without debugger launching it.
 * 0x30040000 is otherwise-unused D2 SRAM3 in both default linker layouts.
 */
#include <das/core.h>
#include <das/hsem.h>
#include <das/result.h>
#include "stm32h755xx.h"
#include <stdint.h>
#include <stdbool.h>

#define BOOT_MAGIC UINT32_C(0x44424f54)
#define CHALLENGE UINT32_C(0x52434d34)
#define RESPONSE UINT32_C(0x434d344f)
#define HSEM_CHANNEL UINT32_C(30)

typedef struct {
    volatile uint32_t magic;
    volatile uint32_t challenge;
    volatile uint32_t response;
    volatile uint32_t cm4_done;
    volatile uint32_t cm4_heartbeat;
} boot_mailbox_t;

#define BOX ((boot_mailbox_t*)(uintptr_t)UINT32_C(0x30040000))

#if defined(CORE_CM7)
typedef struct {
    uint32_t magic;
    volatile uint32_t complete;
    volatile uint32_t error;
    volatile int32_t boot_request;
    volatile uint32_t boot_gate;
    volatile uint32_t domain_ready;
    volatile uint32_t notification;
    volatile uint32_t heartbeat;
} boot_evidence_t;

volatile boot_evidence_t g_das_boot_evidence = {
    .magic = BOOT_MAGIC, .boot_request = DAS_ERROR_NOT_READY,
};

static void fail(uint32_t code) {
    g_das_boot_evidence.error = code;
    for (;;) { __NOP(); }
}

int main(void) {
    boot_mailbox_t* const box = BOX;
    box->magic = BOOT_MAGIC;
    box->challenge = 0u;
    box->response = 0u;
    box->cm4_done = 0u;
    box->cm4_heartbeat = 0u;
    __DMB();
    if (das_hsem_init() != DAS_OK ||
        das_hsem_clear_notification(HSEM_CHANNEL) != DAS_OK ||
        das_hsem_set_notification(HSEM_CHANNEL, true) != DAS_OK) {
        fail(1u);
    }
    box->challenge = CHALLENGE;
    __DMB();
    g_das_boot_evidence.boot_request = das_secondary_core_request_boot();
    if (g_das_boot_evidence.boot_request != DAS_OK) fail(2u);

    bool answered = false;
    for (uint32_t i = 0u; i < UINT32_C(200000000); ++i) {
        if (box->response == RESPONSE && box->cm4_done != 0u) {
            answered = true;
            break;
        }
    }
    if (!answered || box->cm4_heartbeat == 0u) fail(3u);
    bool pending = false;
    if (das_hsem_notification_pending(HSEM_CHANNEL, &pending) != DAS_OK ||
        !pending) fail(4u);
    g_das_boot_evidence.notification = 1u;
    if (das_hsem_clear_notification(HSEM_CHANNEL) != DAS_OK ||
        das_hsem_set_notification(HSEM_CHANNEL, false) != DAS_OK) fail(5u);
    das_secondary_core_state_t state = {0};
    if (das_secondary_core_get_state(&state) != DAS_OK ||
        !state.boot_requested || !state.clock_domain_ready) fail(6u);
    g_das_boot_evidence.boot_gate = 1u;
    g_das_boot_evidence.domain_ready = 1u;
    g_das_boot_evidence.complete = 1u;
    for (;;) { ++g_das_boot_evidence.heartbeat; }
}

void HardFault_Handler(void) { fail(UINT32_C(0xdead)); }

#elif defined(CORE_CM4)
int main(void) {
    boot_mailbox_t* const box = BOX;
    /* If CM4 boots before CM7 (BOOT_CM4 option enabled), wait for CM7. */
    while (box->magic != BOOT_MAGIC || box->challenge != CHALLENGE) {
        __NOP();
    }
    if (das_hsem_init() != DAS_OK ||
        das_hsem_try_acquire(HSEM_CHANNEL) != DAS_OK) {
        for (;;) { __NOP(); }
    }
    box->response = RESPONSE;
    box->cm4_heartbeat = 1u;
    __DMB();
    if (das_hsem_release(HSEM_CHANNEL) != DAS_OK) {
        for (;;) { __NOP(); }
    }
    box->cm4_done = 1u;
    __DMB();
    for (;;) { ++box->cm4_heartbeat; }
}
#else
#error "Autonomous boot fixture requires STM32H755 CM7 or CM4"
#endif
