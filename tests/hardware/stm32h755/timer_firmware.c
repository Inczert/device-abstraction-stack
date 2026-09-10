// SPDX-License-Identifier: Apache-2.0

#include <das/board_resources.h>
#include <das/clock.h>
#include <das/cortex_m/startup.h>
#include <das/gpio.h>
#include <das/irq.h>
#include <das/timer.h>

#include "stm32h755xx.h"

#include <stdbool.h>
#include <stdint.h>

#define DAS_TIMER_TEST_MAGIC UINT32_C(0x44544d52)
#define DAS_TIMER_TEST_HARDFAULT UINT32_C(0xe00a0001)

#define DAS_TIMER_FLAG_HANDLE       (UINT32_C(1) << 0u)
#define DAS_TIMER_FLAG_RUNNING      (UINT32_C(1) << 1u)
#define DAS_TIMER_FLAG_IRQ          (UINT32_C(1) << 2u)
#define DAS_TIMER_FLAG_CADENCE      (UINT32_C(1) << 3u)
#define DAS_TIMER_FLAG_PWM_HANDLE   (UINT32_C(1) << 4u)
#define DAS_TIMER_FLAG_PWM_FREQ     (UINT32_C(1) << 5u)
#define DAS_TIMER_FLAG_PWM_25       (UINT32_C(1) << 6u)
#define DAS_TIMER_FLAG_PWM_50       (UINT32_C(1) << 7u)
#define DAS_TIMER_FLAG_PWM_75       (UINT32_C(1) << 8u)
#define DAS_TIMER_REQUIRED_FLAGS UINT32_C(0x1ff)

extern uint32_t __StackTop;
void TIM2_IRQHandler(void);

__attribute__((section(".isr_vector"), used, aligned(256)))
const uintptr_t g_das_timer_vector_table[] = {
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
    [16 + TIM2_IRQn] = (uintptr_t)&TIM2_IRQHandler,
};

typedef struct das_timer_test_evidence {
    uint32_t magic;
    volatile uint32_t booted;
    volatile uint32_t error;
    volatile uint32_t heartbeat;
    volatile int32_t clock_result;
    volatile int32_t core_clock_result;
    volatile uint32_t core_hz;
    volatile uint32_t flags;
    volatile uint32_t timer_hz;
    volatile uint32_t timer_irq_count;
    volatile uint32_t timer_first_cycle;
    volatile uint32_t timer_last_cycle;
    volatile uint32_t timer_elapsed_cycles;
    volatile uint32_t timer_expected_cycles;
    volatile uint32_t timer_counter_before;
    volatile uint32_t timer_counter_after;
    volatile uint32_t irq_priority_levels;
    volatile uint32_t irq_priority;
    volatile uint32_t pwm_hz;
    volatile uint32_t pwm_period_cycles;
    volatile uint32_t pwm_high_cycles;
    volatile uint32_t pwm_duty_per_mille;
} das_timer_test_evidence_t;

volatile das_timer_test_evidence_t g_das_timer_test_evidence = {
    .magic = DAS_TIMER_TEST_MAGIC,
};

static das_timer_t g_periodic_timer = DAS_TIMER_INVALID;
static volatile uint32_t g_timer_irq_count = 0u;
static volatile uint32_t g_timer_first_cycle = 0u;
static volatile uint32_t g_timer_last_cycle = 0u;

static uint32_t abs_difference(uint32_t a, uint32_t b) {
    return a > b ? a - b : b - a;
}

static bool within_percent(uint32_t actual, uint32_t expected, uint32_t percent) {
    if (expected == 0u) return false;
    const uint64_t tolerance = ((uint64_t)expected * percent) / 100u + 1u;
    return (uint64_t)abs_difference(actual, expected) <= tolerance;
}

static void dwt_start(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __DSB();
    __ISB();
}

static bool wait_level(das_gpio_pin_t pin,
                       bool level,
                       uint32_t timeout_cycles,
                       uint32_t* stamp) {
    const uint32_t start = DWT->CYCCNT;
    while (das_gpio_read_input(pin) != level) {
        if ((uint32_t)(DWT->CYCCNT - start) > timeout_cycles) return false;
    }
    if (stamp != 0) *stamp = DWT->CYCCNT;
    return true;
}

static bool measure_pwm(das_gpio_pin_t input,
                        uint32_t core_hz,
                        uint32_t pwm_hz,
                        uint16_t duty_per_mille,
                        uint32_t* period_cycles,
                        uint32_t* high_cycles) {
    const uint32_t timeout_cycles = core_hz / 10u + 1u;
    uint32_t rise = 0u;
    uint32_t fall = 0u;
    uint32_t next_rise = 0u;

    if (!wait_level(input, false, timeout_cycles, 0) ||
        !wait_level(input, true, timeout_cycles, &rise) ||
        !wait_level(input, false, timeout_cycles, &fall) ||
        !wait_level(input, true, timeout_cycles, &next_rise)) {
        return false;
    }

    const uint32_t period = next_rise - rise;
    const uint32_t high = fall - rise;
    const uint32_t expected_period = pwm_hz == 0u ? 0u : core_hz / pwm_hz;
    const uint32_t expected_high =
        (uint32_t)(((uint64_t)period * duty_per_mille) / UINT64_C(1000));

    if (!within_percent(period, expected_period, 5u) ||
        !within_percent(high, expected_high, 8u)) {
        return false;
    }

    *period_cycles = period;
    *high_cycles = high;
    return true;
}

static bool qualify_pwm_duty(das_pwm_t pwm,
                             das_gpio_pin_t input,
                             uint32_t core_hz,
                             uint32_t pwm_hz,
                             uint16_t duty_per_mille,
                             uint32_t flag) {
    if (das_pwm_set_duty(pwm, duty_per_mille) != DAS_OK) return false;

    uint16_t observed_duty = 0u;
    if (das_pwm_get_duty(pwm, &observed_duty) != DAS_OK ||
        abs_difference(observed_duty, duty_per_mille) > 2u) {
        return false;
    }

    uint32_t period_cycles = 0u;
    uint32_t high_cycles = 0u;
    if (!measure_pwm(input, core_hz, pwm_hz, duty_per_mille,
                     &period_cycles, &high_cycles)) {
        return false;
    }

    g_das_timer_test_evidence.pwm_period_cycles = period_cycles;
    g_das_timer_test_evidence.pwm_high_cycles = high_cycles;
    g_das_timer_test_evidence.pwm_duty_per_mille = observed_duty;
    g_das_timer_test_evidence.flags |= flag;
    return true;
}

int main(void) {
#if defined(CORE_CM7)
    g_das_timer_test_evidence.clock_result =
        das_clock_set_frequency(UINT32_C(400000000));
#else
    g_das_timer_test_evidence.clock_result = DAS_OK;
#endif
    if (g_das_timer_test_evidence.clock_result != DAS_OK) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a01);
        for (;;) { __NOP(); }
    }

    uint32_t core_hz = 0u;
    g_das_timer_test_evidence.core_clock_result = das_clock_get_core_frequency(&core_hz);
    g_das_timer_test_evidence.core_hz = core_hz;
    if (g_das_timer_test_evidence.core_clock_result != DAS_OK || core_hz == 0u) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a02);
        for (;;) { __NOP(); }
    }

    dwt_start();

    const das_timer_config_t timer_config = {.frequency_hz = UINT32_C(1000)};
    if (das_timer_init(&timer_config, &g_periodic_timer) != DAS_OK ||
        !das_timer_is_valid(g_periodic_timer)) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a03);
        for (;;) { __NOP(); }
    }
    g_das_timer_test_evidence.flags |= DAS_TIMER_FLAG_HANDLE;

    if (das_timer_get_frequency(g_periodic_timer,
                                (uint32_t*)&g_das_timer_test_evidence.timer_hz) != DAS_OK ||
        !within_percent(g_das_timer_test_evidence.timer_hz, 1000u, 1u)) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a04);
        for (;;) { __NOP(); }
    }

    das_irq_t irq = DAS_IRQ_INVALID;
    if (das_timer_get_irq(g_periodic_timer, &irq) != DAS_OK || !das_irq_is_valid(irq)) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a05);
        for (;;) { __NOP(); }
    }

    const uint32_t levels = das_irq_priority_levels();
    const uint32_t priority = levels > 1u ? levels / 2u : 0u;
    g_das_timer_test_evidence.irq_priority_levels = levels;
    g_das_timer_test_evidence.irq_priority = priority;
    if (levels == 0u ||
        das_irq_disable(irq) != DAS_OK ||
        das_irq_set_priority(irq, priority) != DAS_OK ||
        das_irq_clear_pending(irq) != DAS_OK ||
        das_timer_clear_update(g_periodic_timer) != DAS_OK ||
        das_timer_update_interrupt_enable(g_periodic_timer, true) != DAS_OK ||
        das_irq_enable(irq) != DAS_OK) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a06);
        for (;;) { __NOP(); }
    }
    g_das_timer_test_evidence.flags |= DAS_TIMER_FLAG_IRQ;

    if (das_timer_get_counter(g_periodic_timer,
                              (uint32_t*)&g_das_timer_test_evidence.timer_counter_before) != DAS_OK ||
        das_timer_start(g_periodic_timer) != DAS_OK) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a07);
        for (;;) { __NOP(); }
    }

    bool running = false;
    if (das_timer_is_running(g_periodic_timer, &running) != DAS_OK || !running) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a08);
        for (;;) { __NOP(); }
    }
    g_das_timer_test_evidence.flags |= DAS_TIMER_FLAG_RUNNING;

    const uint32_t wait_start = DWT->CYCCNT;
    while (g_timer_irq_count < 101u) {
        if ((uint32_t)(DWT->CYCCNT - wait_start) > core_hz) {
            g_das_timer_test_evidence.error = UINT32_C(0x0a09);
            for (;;) { __NOP(); }
        }
    }

    g_das_timer_test_evidence.timer_irq_count = g_timer_irq_count;
    g_das_timer_test_evidence.timer_first_cycle = g_timer_first_cycle;
    g_das_timer_test_evidence.timer_last_cycle = g_timer_last_cycle;
    g_das_timer_test_evidence.timer_elapsed_cycles = g_timer_last_cycle - g_timer_first_cycle;
    g_das_timer_test_evidence.timer_expected_cycles =
        (uint32_t)(((uint64_t)core_hz * UINT64_C(100)) /
                   g_das_timer_test_evidence.timer_hz);

    if (!within_percent(g_das_timer_test_evidence.timer_elapsed_cycles,
                        g_das_timer_test_evidence.timer_expected_cycles,
                        5u)) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a0a);
        for (;;) { __NOP(); }
    }
    g_das_timer_test_evidence.flags |= DAS_TIMER_FLAG_CADENCE;

    if (das_timer_get_counter(g_periodic_timer,
                              (uint32_t*)&g_das_timer_test_evidence.timer_counter_after) != DAS_OK ||
        das_timer_stop(g_periodic_timer) != DAS_OK ||
        das_timer_is_running(g_periodic_timer, &running) != DAS_OK || running ||
        das_timer_update_interrupt_enable(g_periodic_timer, false) != DAS_OK ||
        das_irq_disable(irq) != DAS_OK) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a0b);
        for (;;) { __NOP(); }
    }

    const das_gpio_pin_t pwm_input =
        das_board_gpio_pin(DAS_BOARD_GPIO_ARDUINO_D3);
    if (das_gpio_input_init(pwm_input, DAS_GPIO_PULL_NONE) != DAS_OK) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a10);
        for (;;) { __NOP(); }
    }

    const das_pwm_config_t pwm_config = {
        .frequency_hz = UINT32_C(1000),
        .duty_per_mille = UINT16_C(250),
    };
    das_pwm_t pwm = DAS_PWM_INVALID;
    if (das_board_pwm_init(DAS_BOARD_PWM_ARDUINO_D4, &pwm_config, &pwm) != DAS_OK ||
        !das_pwm_is_valid(pwm)) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a11);
        for (;;) { __NOP(); }
    }
    g_das_timer_test_evidence.flags |= DAS_TIMER_FLAG_PWM_HANDLE;

    uint32_t pwm_hz = 0u;
    if (das_pwm_get_frequency(pwm, &pwm_hz) != DAS_OK ||
        !within_percent(pwm_hz, 1000u, 1u)) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a12);
        for (;;) { __NOP(); }
    }
    g_das_timer_test_evidence.pwm_hz = pwm_hz;
    g_das_timer_test_evidence.flags |= DAS_TIMER_FLAG_PWM_FREQ;

    if (das_pwm_start(pwm) != DAS_OK ||
        das_pwm_is_running(pwm, &running) != DAS_OK || !running) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a13);
        for (;;) { __NOP(); }
    }

    if (!qualify_pwm_duty(pwm, pwm_input, core_hz, pwm_hz, 250u,
                          DAS_TIMER_FLAG_PWM_25)) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a14);
        for (;;) { __NOP(); }
    }
    if (!qualify_pwm_duty(pwm, pwm_input, core_hz, pwm_hz, 500u,
                          DAS_TIMER_FLAG_PWM_50)) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a15);
        for (;;) { __NOP(); }
    }
    if (!qualify_pwm_duty(pwm, pwm_input, core_hz, pwm_hz, 750u,
                          DAS_TIMER_FLAG_PWM_75)) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a16);
        for (;;) { __NOP(); }
    }

    if (das_pwm_stop(pwm) != DAS_OK ||
        das_pwm_is_running(pwm, &running) != DAS_OK || running) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a17);
        for (;;) { __NOP(); }
    }

    if (g_das_timer_test_evidence.flags != DAS_TIMER_REQUIRED_FLAGS) {
        g_das_timer_test_evidence.error = UINT32_C(0x0a18);
        for (;;) { __NOP(); }
    }

    g_das_timer_test_evidence.booted = 1u;
    for (;;) {
        ++g_das_timer_test_evidence.heartbeat;
    }
}

void TIM2_IRQHandler(void) {
    bool pending = false;
    if (das_timer_update_pending(g_periodic_timer, &pending) == DAS_OK && pending) {
        const uint32_t count = g_timer_irq_count + 1u;
        if (count == 1u) g_timer_first_cycle = DWT->CYCCNT;
        if (count == 101u) g_timer_last_cycle = DWT->CYCCNT;
        g_timer_irq_count = count;
        (void)das_timer_clear_update(g_periodic_timer);
    }
}

void HardFault_Handler(void) {
    g_das_timer_test_evidence.error = DAS_TIMER_TEST_HARDFAULT;
    for (;;) { __NOP(); }
}
