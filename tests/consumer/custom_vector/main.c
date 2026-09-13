// SPDX-License-Identifier: Apache-2.0

#include <das/cortex_m/startup.h>

#include <stdint.h>

extern uint32_t __StackTop;

__attribute__((section(".isr_vector"), used, aligned(1024)))
const uintptr_t g_das_vector_table[16] = {
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

int main(void) {
    return 0;
}
