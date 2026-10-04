// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>

#define DAS_FPU_CPACR_ADDRESS UINT32_C(0xE000ED88)
#define DAS_FPU_CFSR_ADDRESS UINT32_C(0xE000ED28)
#define DAS_FPU_CP10_CP11_FULL_ACCESS UINT32_C(0x00F00000)
#define DAS_FPU_CFSR_NOCP UINT32_C(0x00080000)
#define DAS_FPU_EXPECTED_RESULT_BITS UINT32_C(0x40780000)

volatile float g_das_fpu_operand_a = 1.5f;
volatile float g_das_fpu_operand_b = 2.25f;
volatile float g_das_fpu_operand_c = 0.5f;
volatile float g_das_fpu_result;

volatile uint32_t g_das_fpu_started;
volatile uint32_t g_das_fpu_cpacr;
volatile uint32_t g_das_fpu_cfsr;
volatile uint32_t g_das_fpu_result_bits;
volatile uint32_t g_das_fpu_fault;
volatile uint32_t g_das_fpu_pass;
volatile uint32_t g_das_fpu_heartbeat;

static uint32_t float_bits(float value) {
    union {
        float f;
        uint32_t u;
    } conversion = {.f = value};
    return conversion.u;
}

__attribute__((noinline)) static float execute_hardware_fp(void) {
    /*
     * Volatile operands prevent compile-time folding. The qualification scripts
     * also disassemble this ELF and require a real VFP arithmetic instruction.
     */
    const float product = g_das_fpu_operand_a * g_das_fpu_operand_b;
    return product + g_das_fpu_operand_c;
}

int main(void) {
    g_das_fpu_started = 1u;
    g_das_fpu_cpacr =
        *(volatile const uint32_t*)(uintptr_t)DAS_FPU_CPACR_ADDRESS;

    g_das_fpu_result = execute_hardware_fp();
    g_das_fpu_result_bits = float_bits(g_das_fpu_result);
    g_das_fpu_cfsr =
        *(volatile const uint32_t*)(uintptr_t)DAS_FPU_CFSR_ADDRESS;

    if ((g_das_fpu_cpacr & DAS_FPU_CP10_CP11_FULL_ACCESS) ==
            DAS_FPU_CP10_CP11_FULL_ACCESS &&
        (g_das_fpu_cfsr & DAS_FPU_CFSR_NOCP) == 0u &&
        g_das_fpu_result_bits == DAS_FPU_EXPECTED_RESULT_BITS) {
        g_das_fpu_pass = 1u;
    }

    for (;;) {
        ++g_das_fpu_heartbeat;
        __asm volatile("nop");
    }
}

void HardFault_Handler(void) {
    g_das_fpu_fault = 1u;
    g_das_fpu_cfsr =
        *(volatile const uint32_t*)(uintptr_t)DAS_FPU_CFSR_ADDRESS;
    for (;;) {
        __asm volatile("nop");
    }
}

void UsageFault_Handler(void) {
    g_das_fpu_fault = 2u;
    g_das_fpu_cfsr =
        *(volatile const uint32_t*)(uintptr_t)DAS_FPU_CFSR_ADDRESS;
    for (;;) {
        __asm volatile("nop");
    }
}
