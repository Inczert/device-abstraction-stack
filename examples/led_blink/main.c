// SPDX-License-Identifier: Apache-2.0

#include <das/board.h>
#include <das/cortex_m/startup.h>
#include <das/time.h>

#include <stdint.h>

extern uint32_t __StackTop;

/*
 * The application owns its vector table. DAS provides weak Cortex-M startup
 * and exception handlers, while device-specific external IRQ vectors remain an
 * application/RTOS concern. This blinky only needs the core exception table
 * and SysTick used by das_time_init().
 */
__attribute__((section(".isr_vector"), used, aligned(256)))
const uintptr_t g_vector_table[16] = {
    [0] = (uintptr_t)&__StackTop,
    [1] = (uintptr_t)&Reset_Handler,
    [2] = (uintptr_t)&NMI_Handler,
    [3] = (uintptr_t)&HardFault_Handler,
    [4] = (uintptr_t)&MemManage_Handler,
    [5] = (uintptr_t)&BusFault_Handler,
    [6] = (uintptr_t)&UsageFault_Handler,
    [11] = (uintptr_t)&SVC_Handler,
    [12] = (uintptr_t)&DebugMon_Handler,
    [14] = (uintptr_t)&PendSV_Handler,
    [15] = (uintptr_t)&SysTick_Handler,
};

static void halt(void) {
    for (;;) {
    }
}

int main(void) {
    if (das_board_led_init(DAS_BOARD_LED_GREEN, false) != DAS_OK) {
        halt();
    }
    if (das_time_init() != DAS_OK) {
        halt();
    }

    for (;;) {
        if (das_board_led_toggle(DAS_BOARD_LED_GREEN) != DAS_OK) {
            halt();
        }
        if (das_delay_ms(500u) != DAS_OK) {
            halt();
        }
    }
}
