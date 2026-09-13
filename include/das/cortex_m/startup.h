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
 * The device backend supplies one weak default table for the complete MCU IRQ
 * layout. Normal firmware keeps that table and overrides individual weak
 * handler symbols with strong ISR definitions as needed. A bootloader, RTOS or
 * application with a genuinely custom vector/startup policy may instead define
 * a strong g_das_vector_table, or set DAS_USE_DEFAULT_VECTOR_TABLE=OFF and
 * provide its own .isr_vector section.
 */
extern const uintptr_t g_das_vector_table[];

#ifdef __cplusplus
}
#endif

#endif
