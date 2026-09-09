// SPDX-License-Identifier: Apache-2.0

#include <das/cortex_m/startup.h>

#include <stdint.h>

/* Cortex-M System Control Block VTOR register, architecturally defined. */
#define DAS_CORTEX_M_VTOR_ADDRESS UINT32_C(0xE000ED08)

extern uint32_t __data_load__;
extern uint32_t __data_start__;
extern uint32_t __data_end__;
extern uint32_t __bss_start__;
extern uint32_t __bss_end__;
extern uint32_t __vector_table_start__;

extern int main(void);

static inline void cortex_m_dsb(void) {
    __asm volatile("dsb 0xf" ::: "memory");
}

static inline void cortex_m_isb(void) {
    __asm volatile("isb 0xf" ::: "memory");
}

static inline void cortex_m_nop(void) {
    __asm volatile("nop");
}

static void initialize_c_runtime(void) {
    uint32_t* source = &__data_load__;
    uint32_t* destination = &__data_start__;

    while (destination < &__data_end__) {
        *destination++ = *source++;
    }

    destination = &__bss_start__;
    while (destination < &__bss_end__) {
        *destination++ = 0u;
    }
}

__attribute__((weak, noreturn)) void Reset_Handler(void) {
    initialize_c_runtime();

    *(volatile uint32_t*)(uintptr_t)DAS_CORTEX_M_VTOR_ADDRESS =
        (uint32_t)(uintptr_t)&__vector_table_start__;
    cortex_m_dsb();
    cortex_m_isb();

    (void)main();

    for (;;) {
        cortex_m_nop();
    }
}

__attribute__((weak, noreturn)) void Default_Handler(void) {
    for (;;) {
        cortex_m_nop();
    }
}

void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));
