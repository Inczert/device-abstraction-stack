// SPDX-License-Identifier: Apache-2.0

#include <das/cortex_m/startup.h>

#include "stm32h755xx.h"

#include <stdint.h>

#define DAS_CORTEX_M_CORE_VECTOR_COUNT UINT32_C(16)
#define DAS_STM32H755_EXTERNAL_VECTOR_COUNT ((uint32_t)WAKEUP_PIN_IRQn + UINT32_C(1))
#define DAS_STM32H755_VECTOR_COUNT \
    (DAS_CORTEX_M_CORE_VECTOR_COUNT + DAS_STM32H755_EXTERNAL_VECTOR_COUNT)

_Static_assert(WAKEUP_PIN_IRQn >= 0, "STM32H755 final IRQ number must be external");

/*
 * Safe default vector table for simple bare-metal DAS applications.
 *
 * CMSIS supplies the STM32H755 IRQ numbering; DAS supplies the actual table so
 * applications do not need to copy startup boilerplate merely to boot and use
 * the default SysTick timebase. Device external IRQ slots default to
 * Default_Handler until an application supplies its own vector table.
 *
 * The symbol is weak deliberately. An application can provide a strong
 * g_das_vector_table in .isr_vector to replace this table without rebuilding
 * DAS. DAS_USE_DEFAULT_VECTOR_TABLE=OFF also stops CMake from force-linking this
 * archive member when a completely custom vector/startup policy is used.
 */
__attribute__((weak, used, section(".isr_vector"), aligned(1024)))
const uintptr_t g_das_vector_table[DAS_STM32H755_VECTOR_COUNT] = {
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
    [16 ... DAS_STM32H755_VECTOR_COUNT - 1] = (uintptr_t)&Default_Handler,
};
