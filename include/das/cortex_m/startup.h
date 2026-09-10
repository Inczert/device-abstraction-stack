// SPDX-License-Identifier: Apache-2.0
#ifndef DAS_CORTEX_M_STARTUP_H
#define DAS_CORTEX_M_STARTUP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Optional Cortex-M startup/exception entry points.
 *
 * The default implementations are weak so a bootloader, RTOS, or application
 * can provide strong replacements without modifying DAS.
 */
void Reset_Handler(void);
void Default_Handler(void);
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

/*
 * Canonical DAS vector-table symbol.
 *
 * The STM32H755 backend supplies a weak default table. Applications that need
 * custom external IRQ bindings can provide a strong definition with this name
 * in their own .isr_vector section. DAS_USE_DEFAULT_VECTOR_TABLE=OFF disables
 * force-linking of the default archive member entirely.
 */
extern const uintptr_t g_das_vector_table[];

#ifdef __cplusplus
}
#endif

#endif
