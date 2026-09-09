// SPDX-License-Identifier: Apache-2.0

#include <das/clock.h>
#include <das/cortex_m/startup.h>
#include <das/result.h>

#include "clock_internal.h"
#include "stm32h755xx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DAS_CLOCK_TEST_MAGIC UINT32_C(0x44415343)

#define DAS_CLOCK_PROFILE_64  (UINT32_C(1) << 0u)
#define DAS_CLOCK_PROFILE_200 (UINT32_C(1) << 1u)
#define DAS_CLOCK_PROFILE_300 (UINT32_C(1) << 2u)
#define DAS_CLOCK_PROFILE_400 (UINT32_C(1) << 3u)
#define DAS_CLOCK_PROFILE_ALL (DAS_CLOCK_PROFILE_64 | DAS_CLOCK_PROFILE_200 | \
                               DAS_CLOCK_PROFILE_300 | DAS_CLOCK_PROFILE_400)

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
    volatile int32_t unsupported_result;
    volatile uint32_t failed_frequency_hz;
    volatile uint32_t profile_mask;
    volatile uint32_t supported_count;
    volatile uint32_t system_hz;
    volatile uint32_t cm7_hz;
    volatile uint32_t cm4_hz;
    volatile uint32_t ahb_hz;
    volatile uint32_t apb1_hz;
    volatile uint32_t apb2_hz;
    volatile uint32_t apb3_hz;
    volatile uint32_t apb4_hz;
    volatile uint32_t pwr_cr3;
    volatile uint32_t pwr_csr1;
    volatile uint32_t pwr_d3cr;
    volatile uint32_t power_ready;
    volatile uint32_t heartbeat;
} das_clock_test_evidence_t;

volatile das_clock_test_evidence_t g_das_clock_test_evidence = {
    .magic = DAS_CLOCK_TEST_MAGIC,
    .apply_result = DAS_ERROR_UNSUPPORTED,
    .query_result = DAS_ERROR_UNSUPPORTED,
    .unsupported_result = DAS_OK,
};

static bool test_profile(uint32_t frequency_hz, uint32_t flag) {
    const das_result_t apply_result = das_clock_set_frequency(frequency_hz);
    g_das_clock_test_evidence.apply_result = apply_result;
    if (apply_result != DAS_OK) {
        g_das_clock_test_evidence.failed_frequency_hz = frequency_hz;
        return false;
    }

    uint32_t observed_hz = 0u;
    const das_result_t query_result = das_clock_get_frequency(&observed_hz);
    g_das_clock_test_evidence.query_result = query_result;
    if (query_result != DAS_OK || observed_hz != frequency_hz) {
        g_das_clock_test_evidence.failed_frequency_hz = frequency_hz;
        return false;
    }

    g_das_clock_test_evidence.profile_mask |= flag;
    return true;
}

int main(void) {
    uint32_t supported[4] = {0};
    g_das_clock_test_evidence.supported_count =
        (uint32_t)das_clock_get_supported_frequencies(
            supported,
            sizeof(supported) / sizeof(supported[0]));

    const bool supported_list_ok =
        g_das_clock_test_evidence.supported_count == 4u &&
        supported[0] == UINT32_C(64000000) &&
        supported[1] == UINT32_C(200000000) &&
        supported[2] == UINT32_C(300000000) &&
        supported[3] == UINT32_C(400000000) &&
        das_clock_frequency_supported(UINT32_C(64000000)) &&
        das_clock_frequency_supported(UINT32_C(200000000)) &&
        das_clock_frequency_supported(UINT32_C(300000000)) &&
        das_clock_frequency_supported(UINT32_C(400000000)) &&
        !das_clock_frequency_supported(UINT32_C(480000000));

    if (!supported_list_ok) {
        g_das_clock_test_evidence.failed_frequency_hz = UINT32_C(1);
        g_das_clock_test_evidence.booted = 1u;
        for (;;) {
            ++g_das_clock_test_evidence.heartbeat;
        }
    }

    if (!test_profile(UINT32_C(64000000), DAS_CLOCK_PROFILE_64) ||
        !test_profile(UINT32_C(200000000), DAS_CLOCK_PROFILE_200) ||
        !test_profile(UINT32_C(300000000), DAS_CLOCK_PROFILE_300) ||
        !test_profile(UINT32_C(400000000), DAS_CLOCK_PROFILE_400)) {
        g_das_clock_test_evidence.booted = 1u;
        for (;;) {
            ++g_das_clock_test_evidence.heartbeat;
        }
    }

    g_das_clock_test_evidence.unsupported_result =
        das_clock_set_frequency(UINT32_C(480000000));

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

    g_das_clock_test_evidence.pwr_cr3 = PWR->CR3;
    g_das_clock_test_evidence.pwr_csr1 = PWR->CSR1;
    g_das_clock_test_evidence.pwr_d3cr = PWR->D3CR;
    g_das_clock_test_evidence.power_ready =
        ((PWR->CR3 & (PWR_CR3_SMPSEN | PWR_CR3_LDOEN | PWR_CR3_BYPASS)) ==
         PWR_CR3_SMPSEN) &&
        ((PWR->CSR1 & PWR_CSR1_ACTVOSRDY) != 0u) &&
        ((PWR->D3CR & PWR_D3CR_VOSRDY) != 0u);

    if (g_das_clock_test_evidence.profile_mask != DAS_CLOCK_PROFILE_ALL) {
        g_das_clock_test_evidence.failed_frequency_hz = UINT32_C(2);
    }

    g_das_clock_test_evidence.booted = 1u;

    for (;;) {
        ++g_das_clock_test_evidence.heartbeat;
    }
}
