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

/*
 * Optional internal SysTick hook. When the DAS Cortex-M time backend is linked,
 * it provides this symbol. RTOS/application code can still replace the weak
 * SysTick_Handler entirely and use das_time_set_source() instead.
 */
void das_cortex_m_systick_hook(void) __attribute__((weak));

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

#define DAS_CORTEX_M_DEFAULT_HANDLER(name) \
    void name(void) __attribute__((weak, alias("Default_Handler"), noreturn))

DAS_CORTEX_M_DEFAULT_HANDLER(NMI_Handler);
DAS_CORTEX_M_DEFAULT_HANDLER(HardFault_Handler);
DAS_CORTEX_M_DEFAULT_HANDLER(MemManage_Handler);
DAS_CORTEX_M_DEFAULT_HANDLER(BusFault_Handler);
DAS_CORTEX_M_DEFAULT_HANDLER(UsageFault_Handler);
DAS_CORTEX_M_DEFAULT_HANDLER(SVC_Handler);
DAS_CORTEX_M_DEFAULT_HANDLER(DebugMon_Handler);
DAS_CORTEX_M_DEFAULT_HANDLER(PendSV_Handler);

__attribute__((weak)) void SysTick_Handler(void) {
    if (das_cortex_m_systick_hook != 0) {
        das_cortex_m_systick_hook();
    }
}
