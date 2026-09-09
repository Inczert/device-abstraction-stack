// SPDX-License-Identifier: Apache-2.0

#include <das/cortex_m/startup.h>
#include <das/result.h>

#include "clock_internal.h"

#include <stdint.h>

#define DAS_CLOCK_TEST_MAGIC UINT32_C(0x44415343)

extern uint32_t __StackTop;

__attribute__((section(".isr_vector"), used, aligned(256)))
const uintptr_t g_das_clock_vector_table[16] = {
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

typedef struct das_clock_test_evidence {
    uint32_t magic;
    volatile uint32_t booted;
    volatile int32_t apply_result;
    volatile int32_t query_result;
    volatile uint32_t system_hz;
    volatile uint32_t cm7_hz;
    volatile uint32_t cm4_hz;
    volatile uint32_t ahb_hz;
    volatile uint32_t apb1_hz;
    volatile uint32_t apb2_hz;
    volatile uint32_t apb3_hz;
    volatile uint32_t apb4_hz;
    volatile uint32_t heartbeat;
} das_clock_test_evidence_t;

volatile das_clock_test_evidence_t g_das_clock_test_evidence = {
    .magic = DAS_CLOCK_TEST_MAGIC,
    .apply_result = DAS_ERROR_UNSUPPORTED,
    .query_result = DAS_ERROR_UNSUPPORTED,
};

int main(void) {
    const stm32h755_clock_config_t config = {
        .source = STM32H755_CLOCK_SOURCE_HSI,
        .source_hz = 0u,
        .use_pll1 = true,
        .pll_m = 8u,
        .pll_n = 100u,
        .pll_p = 2u,
        .pll_q = 4u,
        .pll_r = 2u,
        .d1_core_divider = 1u,
        .ahb_divider = 2u,
        .apb1_divider = 2u,
        .apb2_divider = 2u,
        .apb3_divider = 2u,
        .apb4_divider = 2u,
        .wait_limit = UINT32_C(1000000),
    };

    const das_result_t apply_result = stm32h755_clock_apply(&config);
    g_das_clock_test_evidence.apply_result = apply_result;

    stm32h755_clock_frequencies_t frequencies = {0};
    const das_result_t query_result =
        stm32h755_clock_get_frequencies(0u, &frequencies);
    g_das_clock_test_evidence.query_result = query_result;
    g_das_clock_test_evidence.system_hz = frequencies.system_hz;
    g_das_clock_test_evidence.cm7_hz = frequencies.cm7_hz;
    g_das_clock_test_evidence.cm4_hz = frequencies.cm4_hz;
    g_das_clock_test_evidence.ahb_hz = frequencies.ahb_hz;
    g_das_clock_test_evidence.apb1_hz = frequencies.apb1_hz;
    g_das_clock_test_evidence.apb2_hz = frequencies.apb2_hz;
    g_das_clock_test_evidence.apb3_hz = frequencies.apb3_hz;
    g_das_clock_test_evidence.apb4_hz = frequencies.apb4_hz;
    g_das_clock_test_evidence.booted = 1u;

    for (;;) {
        ++g_das_clock_test_evidence.heartbeat;
    }
}
