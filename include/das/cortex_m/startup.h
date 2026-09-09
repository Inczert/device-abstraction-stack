// SPDX-License-Identifier: Apache-2.0
#ifndef DAS_CORTEX_M_STARTUP_H
#define DAS_CORTEX_M_STARTUP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Optional Cortex-M startup/exception entry points.
 *
 * The default implementations are weak so a bootloader, RTOS, or application
 * can provide strong replacements without modifying DAS.
 *
 * Device-specific external interrupt vectors do not belong to this layer.
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

#ifdef __cplusplus
}
#endif

#endif
