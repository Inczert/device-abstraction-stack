// SPDX-License-Identifier: Apache-2.0

#include <das/clock.h>
#include <das/cortex_m/startup.h>
#include <das/time.h>

#include "stm32h755xx.h"

#include <stdbool.h>
#include <stdint.h>

#define DAS_TIME_TEST_MAGIC UINT32_C(0x44415354)
#define DAS_TIME_TEST_DELAY_MS UINT32_C(100)

#define DAS_TIME_FLAG_EXTERNAL_SOURCE (UINT32_C(1) << 0u)
#define DAS_TIME_FLAG_WRAP_ELAPSED     (UINT32_C(1) << 1u)
#define DAS_TIME_FLAG_DEADLINE_BEFORE  (UINT32_C(1) << 2u)
#define DAS_TIME_FLAG_DEADLINE_REACHED (UINT32_C(1) << 3u)
#define DAS_TIME_FLAG_RANGE_REJECTED   (UINT32_C(1) << 4u)
#define DAS_TIME_FLAGS_EXPECTED        UINT32_C(0x1f)

extern uint32_t __StackTop;

__attribute__((section(".isr_vector"), used, aligned(256)))
const uintptr_t g_das_time_vector_table[16] = {
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

typedef struct das_time_test_evidence {
    uint32_t magic;
    volatile uint32_t booted;
    volatile uint32_t error;
    volatile int32_t not_ready_result;
    volatile int32_t source_result;
    volatile int32_t clock_result;
    volatile int32_t core_query_result;
    volatile int32_t init_result;
    volatile int32_t delay_result;
    volatile int32_t invalid_deadline_result;
    volatile uint32_t wrap_flags;
    volatile uint32_t wrap_elapsed_ms;
    volatile uint32_t wrap_deadline_ms;
    volatile uint32_t core_hz;
    volatile uint32_t start_ms;
    volatile uint32_t end_ms;
    volatile uint32_t elapsed_ms;
    volatile uint32_t measured_cycles;
    volatile uint32_t expected_cycles;
    volatile uint32_t tolerance_cycles;
    volatile uint32_t timing_ok;
    volatile uint32_t source_ready;
    volatile uint32_t heartbeat;
} das_time_test_evidence_t;

static uint32_t g_fake_time_ms;

volatile das_time_test_evidence_t g_das_time_test_evidence = {
    .magic = DAS_TIME_TEST_MAGIC,
    .not_ready_result = DAS_ERROR_UNSUPPORTED,
    .source_result = DAS_ERROR_UNSUPPORTED,
    .clock_result = DAS_ERROR_UNSUPPORTED,
    .core_query_result = DAS_ERROR_UNSUPPORTED,
    .init_result = DAS_ERROR_UNSUPPORTED,
    .delay_result = DAS_ERROR_UNSUPPORTED,
    .invalid_deadline_result = DAS_ERROR_UNSUPPORTED,
};

static das_time_ms_t fake_time_source(void* context) {
    return *(const uint32_t*)context;
}

static void run_wrap_tests(void) {
    g_fake_time_ms = UINT32_MAX - UINT32_C(5);
    g_das_time_test_evidence.source_result =
        das_time_set_source(fake_time_source, &g_fake_time_ms);
    if (g_das_time_test_evidence.source_result != DAS_OK ||
        !das_time_is_ready()) {
        g_das_time_test_evidence.error = UINT32_C(0x5001);
        return;
    }
    g_das_time_test_evidence.wrap_flags |= DAS_TIME_FLAG_EXTERNAL_SOURCE;

    const das_time_ms_t start_ms = das_time_now_ms();
    das_time_ms_t deadline_ms = 0u;
    if (das_time_deadline_after(8u, &deadline_ms) != DAS_OK ||
        deadline_ms != 2u) {
        g_das_time_test_evidence.error = UINT32_C(0x5002);
        return;
    }
    g_das_time_test_evidence.wrap_deadline_ms = deadline_ms;

    g_fake_time_ms = 1u;
    if (!das_time_deadline_reached(deadline_ms)) {
        g_das_time_test_evidence.wrap_flags |= DAS_TIME_FLAG_DEADLINE_BEFORE;
    } else {
        g_das_time_test_evidence.error = UINT32_C(0x5003);
        return;
    }

    g_fake_time_ms = 2u;
    if (das_time_deadline_reached(deadline_ms)) {
        g_das_time_test_evidence.wrap_flags |= DAS_TIME_FLAG_DEADLINE_REACHED;
    } else {
        g_das_time_test_evidence.error = UINT32_C(0x5004);
        return;
    }

    g_fake_time_ms = 3u;
    g_das_time_test_evidence.wrap_elapsed_ms = das_time_elapsed_ms(start_ms);
    if (g_das_time_test_evidence.wrap_elapsed_ms == 9u &&
        das_time_interval_elapsed(start_ms, 9u)) {
        g_das_time_test_evidence.wrap_flags |= DAS_TIME_FLAG_WRAP_ELAPSED;
    } else {
        g_das_time_test_evidence.error = UINT32_C(0x5005);
        return;
    }

    das_time_ms_t ignored_deadline = 0u;
    g_das_time_test_evidence.invalid_deadline_result =
        das_time_deadline_after(DAS_TIME_MAX_INTERVAL_MS + 1u,
                                &ignored_deadline);
    if (g_das_time_test_evidence.invalid_deadline_result ==
        DAS_ERROR_INVALID_ARGUMENT) {
        g_das_time_test_evidence.wrap_flags |= DAS_TIME_FLAG_RANGE_REJECTED;
    } else {
        g_das_time_test_evidence.error = UINT32_C(0x5006);
    }
}

static void enable_cycle_counter(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __DSB();
    __ISB();
}

static void run_default_timebase_test(void) {
#if defined(CORE_CM7)
    g_das_time_test_evidence.clock_result =
        das_clock_set_frequency(UINT32_C(400000000));
#elif defined(CORE_CM4)
    /* CPU2 does not own the global clock tree; use the live reset/current tree. */
    g_das_time_test_evidence.clock_result = DAS_OK;
#else
#error "STM32H755 time test requires CORE_CM7 or CORE_CM4"
#endif
    if (g_das_time_test_evidence.clock_result != DAS_OK) {
        g_das_time_test_evidence.error = UINT32_C(0x5010);
        return;
    }

    uint32_t core_hz = 0u;
    g_das_time_test_evidence.core_query_result =
        das_clock_get_core_frequency(&core_hz);
    g_das_time_test_evidence.core_hz = core_hz;
    if (g_das_time_test_evidence.core_query_result != DAS_OK || core_hz == 0u) {
        g_das_time_test_evidence.error = UINT32_C(0x5011);
        return;
    }

    g_das_time_test_evidence.init_result = das_time_init();
    g_das_time_test_evidence.source_ready = das_time_is_ready() ? 1u : 0u;
    if (g_das_time_test_evidence.init_result != DAS_OK ||
        g_das_time_test_evidence.source_ready == 0u) {
        g_das_time_test_evidence.error = UINT32_C(0x5012);
        return;
    }

    enable_cycle_counter();

    g_das_time_test_evidence.start_ms = das_time_now_ms();
    const uint32_t cycle_start = DWT->CYCCNT;
    g_das_time_test_evidence.delay_result =
        das_delay_ms(DAS_TIME_TEST_DELAY_MS);
    const uint32_t cycle_end = DWT->CYCCNT;
    g_das_time_test_evidence.end_ms = das_time_now_ms();

    g_das_time_test_evidence.elapsed_ms =
        (uint32_t)(g_das_time_test_evidence.end_ms -
                   g_das_time_test_evidence.start_ms);
    g_das_time_test_evidence.measured_cycles = cycle_end - cycle_start;
    g_das_time_test_evidence.expected_cycles =
        (core_hz / UINT32_C(1000)) * DAS_TIME_TEST_DELAY_MS;
    g_das_time_test_evidence.tolerance_cycles =
        g_das_time_test_evidence.expected_cycles / UINT32_C(20);

    const uint32_t lower =
        g_das_time_test_evidence.expected_cycles -
        g_das_time_test_evidence.tolerance_cycles;
    const uint32_t upper =
        g_das_time_test_evidence.expected_cycles +
        g_das_time_test_evidence.tolerance_cycles;

    if (g_das_time_test_evidence.delay_result == DAS_OK &&
        g_das_time_test_evidence.elapsed_ms >= DAS_TIME_TEST_DELAY_MS &&
        g_das_time_test_evidence.elapsed_ms <= DAS_TIME_TEST_DELAY_MS + 2u &&
        g_das_time_test_evidence.measured_cycles >= lower &&
        g_das_time_test_evidence.measured_cycles <= upper) {
        g_das_time_test_evidence.timing_ok = 1u;
    } else {
        g_das_time_test_evidence.error = UINT32_C(0x5013);
    }
}

int main(void) {
    g_das_time_test_evidence.not_ready_result = das_delay_ms(1u);
    if (g_das_time_test_evidence.not_ready_result != DAS_ERROR_NOT_READY) {
        g_das_time_test_evidence.error = UINT32_C(0x5000);
    }

    if (g_das_time_test_evidence.error == 0u) {
        run_wrap_tests();
    }
    if (g_das_time_test_evidence.error == 0u &&
        g_das_time_test_evidence.wrap_flags == DAS_TIME_FLAGS_EXPECTED) {
        run_default_timebase_test();
    }

    g_das_time_test_evidence.booted = 1u;
    for (;;) {
        ++g_das_time_test_evidence.heartbeat;
    }
}
