// SPDX-License-Identifier: Apache-2.0

#include <das/board_resources.h>
#include <das/clock.h>
#include <das/cortex_m/startup.h>
#include <das/gpio.h>
#include <das/i2c.h>
#include <das/time.h>

#include "stm32h755xx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DAS_I2C_TEST_MAGIC UINT32_C(0x44493243)
#define DAS_I2C_TEST_HARDFAULT UINT32_C(0xe00c0001)
#define DAS_I2C_TARGET_ADDRESS UINT8_C(0x52)
#define DAS_I2C_MISSING_ADDRESS UINT8_C(0x53)

#define DAS_I2C_FLAG_HANDLE       (UINT32_C(1) << 0u)
#define DAS_I2C_FLAG_STANDARD     (UINT32_C(1) << 1u)
#define DAS_I2C_FLAG_PROBE        (UINT32_C(1) << 2u)
#define DAS_I2C_FLAG_NACK         (UINT32_C(1) << 3u)
#define DAS_I2C_FLAG_WRITE        (UINT32_C(1) << 4u)
#define DAS_I2C_FLAG_READ         (UINT32_C(1) << 5u)
#define DAS_I2C_FLAG_WRITE_READ   (UINT32_C(1) << 6u)
#define DAS_I2C_FLAG_FAST         (UINT32_C(1) << 7u)
#define DAS_I2C_FLAG_TARGET       (UINT32_C(1) << 8u)
#define DAS_I2C_REQUIRED_FLAGS UINT32_C(0x1ff)

#if defined(CORE_CM7)
#define DAS_RCC_CORE RCC_C1
#elif defined(CORE_CM4)
#define DAS_RCC_CORE RCC_C2
#else
#error "I2C hardware test requires CORE_CM7 or CORE_CM4"
#endif

extern uint32_t __StackTop;
void I2C4_EV_IRQHandler(void);
void I2C4_ER_IRQHandler(void);

__attribute__((section(".isr_vector"), used, aligned(256)))
const uintptr_t g_das_i2c_vector_table[] = {
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
    [16 + I2C4_EV_IRQn] = (uintptr_t)&I2C4_EV_IRQHandler,
    [16 + I2C4_ER_IRQn] = (uintptr_t)&I2C4_ER_IRQHandler,
};

typedef struct das_i2c_test_evidence {
    uint32_t magic;
    volatile uint32_t booted;
    volatile uint32_t error;
    volatile uint32_t heartbeat;
    volatile int32_t clock_result;
    volatile int32_t core_clock_result;
    volatile int32_t time_result;
    volatile uint32_t core_hz;
    volatile uint32_t flags;
    volatile uint32_t standard_hz;
    volatile uint32_t fast_hz;
    volatile int32_t missing_probe_result;
    volatile uint32_t bytes_checked;
    volatile uint32_t target_address_events;
    volatile uint32_t target_stop_events;
    volatile uint32_t target_error_events;
    volatile uint32_t mismatch_index;
    volatile uint32_t mismatch_expected;
    volatile uint32_t mismatch_actual;
} das_i2c_test_evidence_t;

volatile das_i2c_test_evidence_t g_das_i2c_test_evidence = {
    .magic = DAS_I2C_TEST_MAGIC,
};

static volatile uint8_t g_target_rx[64];
static volatile uint32_t g_target_rx_count;
static volatile uint32_t g_target_tx_index;
static volatile uint8_t g_target_register;

static uint8_t target_value(uint32_t index) {
    return (uint8_t)((index * UINT32_C(37) + UINT32_C(0x23)) & UINT32_C(0xff));
}

static bool frequency_close(uint32_t actual, uint32_t requested) {
    const uint32_t difference = actual > requested ? actual - requested : requested - actual;
    return difference <= requested / 20u + 1u; /* 5% nominal timing tolerance. */
}

static void target_clear_state(void) {
    g_target_rx_count = 0u;
    g_target_tx_index = 0u;
    g_target_register = 0u;
    for (size_t index = 0u; index < sizeof(g_target_rx); ++index) {
        g_target_rx[index] = 0u;
    }
}

static void target_flush_txdr(void) {
    uint32_t status = I2C4->ISR;

    /*
     * A target transmitter may have already queued the next byte when the
     * controller terminates a read with NACK/STOP. STM32 keeps that byte in
     * TXDR and can emit it as the first byte of a later transaction unless
     * TXDR is explicitly flushed. TXE is software-writable for this purpose.
     */
    if ((status & I2C_ISR_TXIS) != 0u) {
        I2C4->TXDR = 0u;
        status = I2C4->ISR;
    }
    if ((status & I2C_ISR_TXE) == 0u) {
        I2C4->ISR |= I2C_ISR_TXE;
    }
}

static das_result_t target_configure_pins(void) {
    const das_gpio_config_t config = {
        .mode = DAS_GPIO_MODE_ALTERNATE,
        .pull = DAS_GPIO_PULL_UP,
        .output_type = DAS_GPIO_OUTPUT_OPEN_DRAIN,
        .speed = DAS_GPIO_SPEED_HIGH,
        .alternate = 4u,
        .initial_high = true,
    };
    das_result_t result = das_gpio_configure((das_gpio_pin_t){DAS_GPIO_PORT_F, 14u}, &config);
    if (result != DAS_OK) return result;
    return das_gpio_configure((das_gpio_pin_t){DAS_GPIO_PORT_F, 15u}, &config);
}

static das_result_t target_init(uint32_t timing) {
    das_result_t result = target_configure_pins();
    if (result != DAS_OK) return result;

#if defined(RCC_D3CCIPR_I2C4SEL)
    RCC->D3CCIPR =
        (RCC->D3CCIPR & ~RCC_D3CCIPR_I2C4SEL) |
        RCC_D3CCIPR_I2C4SEL_1;
#else
#error "STM32H755 CMSIS header does not expose I2C4 kernel-clock selection"
#endif

    DAS_RCC_CORE->APB4ENR |= RCC_APB4ENR_I2C4EN;
    (void)DAS_RCC_CORE->APB4ENR;
    __DSB();

    NVIC_DisableIRQ(I2C4_EV_IRQn);
    NVIC_DisableIRQ(I2C4_ER_IRQn);

    I2C4->CR1 &= ~I2C_CR1_PE;
    I2C4->CR1 = 0u;
    I2C4->CR2 = 0u;
    I2C4->OAR1 =
        (((uint32_t)DAS_I2C_TARGET_ADDRESS << 1u) & I2C_OAR1_OA1) |
        I2C_OAR1_OA1EN;
    I2C4->OAR2 = 0u;
    I2C4->TIMINGR = timing;
    I2C4->TIMEOUTR = 0u;
    I2C4->ICR =
        I2C_ICR_ADDRCF | I2C_ICR_NACKCF | I2C_ICR_STOPCF |
        I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF;

    target_clear_state();
    I2C4->CR1 =
        I2C_CR1_PE | I2C_CR1_ADDRIE | I2C_CR1_RXIE | I2C_CR1_TXIE |
        I2C_CR1_STOPIE | I2C_CR1_NACKIE | I2C_CR1_ERRIE;

    NVIC_SetPriority(I2C4_EV_IRQn, 4u);
    NVIC_SetPriority(I2C4_ER_IRQn, 4u);
    NVIC_EnableIRQ(I2C4_EV_IRQn);
    NVIC_EnableIRQ(I2C4_ER_IRQn);
    return DAS_OK;
}

static void target_update_timing(uint32_t timing) {
    const uint32_t cr1 = I2C4->CR1;
    I2C4->CR1 = cr1 & ~I2C_CR1_PE;
    I2C4->TIMINGR = timing;
    I2C4->CR1 = cr1;
}

void I2C4_EV_IRQHandler(void) {
    uint32_t status = I2C4->ISR;

    if ((status & I2C_ISR_ADDR) != 0u) {
        ++g_das_i2c_test_evidence.target_address_events;
        if ((status & I2C_ISR_DIR) != 0u) {
            g_target_tx_index = 0u;
        } else {
            g_target_rx_count = 0u;
        }
        I2C4->ICR = I2C_ICR_ADDRCF;
        status = I2C4->ISR;
    }

    if ((status & I2C_ISR_RXNE) != 0u) {
        const uint8_t value = (uint8_t)I2C4->RXDR;
        const uint32_t index = g_target_rx_count;
        if (index < sizeof(g_target_rx)) {
            g_target_rx[index] = value;
        }
        if (index == 0u) {
            g_target_register = value;
        }
        g_target_rx_count = index + 1u;
        status = I2C4->ISR;
    }

    if ((status & I2C_ISR_NACKF) != 0u) {
        target_flush_txdr();
        I2C4->ICR = I2C_ICR_NACKCF;
        status = I2C4->ISR;
    }

    if ((status & I2C_ISR_TXIS) != 0u) {
        I2C4->TXDR = target_value((uint32_t)g_target_register + g_target_tx_index);
        ++g_target_tx_index;
        status = I2C4->ISR;
    }

    if ((status & I2C_ISR_STOPF) != 0u) {
        target_flush_txdr();
        ++g_das_i2c_test_evidence.target_stop_events;
        I2C4->ICR = I2C_ICR_STOPCF;
    }
}

void I2C4_ER_IRQHandler(void) {
    const uint32_t status = I2C4->ISR;
    uint32_t clear = 0u;
    if ((status & I2C_ISR_BERR) != 0u) clear |= I2C_ICR_BERRCF;
    if ((status & I2C_ISR_ARLO) != 0u) clear |= I2C_ICR_ARLOCF;
    if ((status & I2C_ISR_OVR) != 0u) clear |= I2C_ICR_OVRCF;
    if (clear != 0u) {
        ++g_das_i2c_test_evidence.target_error_events;
        I2C4->ICR = clear;
    }
}

static bool wait_target_rx(uint32_t expected) {
    const das_time_ms_t start = das_time_now_ms();
    while (g_target_rx_count < expected) {
        if (das_time_elapsed_ms(start) >= 20u) return false;
    }
    return true;
}

static bool check_target_write(const uint8_t* expected, size_t size) {
    if (!wait_target_rx((uint32_t)size)) return false;
    for (size_t index = 0u; index < size; ++index) {
        const uint8_t actual = g_target_rx[index];
        if (actual != expected[index]) {
            g_das_i2c_test_evidence.mismatch_index = (uint32_t)index;
            g_das_i2c_test_evidence.mismatch_expected = expected[index];
            g_das_i2c_test_evidence.mismatch_actual = actual;
            return false;
        }
        ++g_das_i2c_test_evidence.bytes_checked;
    }
    return true;
}

static bool check_target_read(const uint8_t* actual,
                              size_t size,
                              uint8_t start_register) {
    for (size_t index = 0u; index < size; ++index) {
        const uint8_t expected = target_value((uint32_t)start_register + (uint32_t)index);
        if (actual[index] != expected) {
            g_das_i2c_test_evidence.mismatch_index = (uint32_t)index;
            g_das_i2c_test_evidence.mismatch_expected = expected;
            g_das_i2c_test_evidence.mismatch_actual = actual[index];
            return false;
        }
        ++g_das_i2c_test_evidence.bytes_checked;
    }
    return true;
}

int main(void) {
#if defined(CORE_CM7)
    g_das_i2c_test_evidence.clock_result = das_clock_set_frequency(UINT32_C(400000000));
#else
    g_das_i2c_test_evidence.clock_result = DAS_OK;
#endif
    if (g_das_i2c_test_evidence.clock_result != DAS_OK) {
        g_das_i2c_test_evidence.error = UINT32_C(0x0c01);
        for (;;) { __NOP(); }
    }

    uint32_t core_hz = 0u;
    g_das_i2c_test_evidence.core_clock_result = das_clock_get_core_frequency(&core_hz);
    g_das_i2c_test_evidence.core_hz = core_hz;
    if (g_das_i2c_test_evidence.core_clock_result != DAS_OK) {
        g_das_i2c_test_evidence.error = UINT32_C(0x0c02);
        for (;;) { __NOP(); }
    }

    g_das_i2c_test_evidence.time_result = das_time_init();
    if (g_das_i2c_test_evidence.time_result != DAS_OK) {
        g_das_i2c_test_evidence.error = UINT32_C(0x0c03);
        for (;;) { __NOP(); }
    }

    const das_i2c_config_t standard = {.frequency_hz = UINT32_C(100000)};
    das_i2c_t i2c = DAS_I2C_INVALID;
    das_result_t result = das_board_i2c_init(DAS_BOARD_I2C_ARDUINO, &standard, &i2c);
    if (result != DAS_OK || !das_i2c_is_valid(i2c)) {
        g_das_i2c_test_evidence.error = UINT32_C(0x0c10) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    g_das_i2c_test_evidence.flags |= DAS_I2C_FLAG_HANDLE;

    result = das_i2c_get_frequency(i2c, (uint32_t*)&g_das_i2c_test_evidence.standard_hz);
    if (result != DAS_OK || !frequency_close(g_das_i2c_test_evidence.standard_hz, standard.frequency_hz)) {
        g_das_i2c_test_evidence.error = UINT32_C(0x0c20) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    g_das_i2c_test_evidence.flags |= DAS_I2C_FLAG_STANDARD;

    result = target_init(I2C1->TIMINGR);
    if (result != DAS_OK) {
        g_das_i2c_test_evidence.error = UINT32_C(0x0c30) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    g_das_i2c_test_evidence.flags |= DAS_I2C_FLAG_TARGET;

    result = das_i2c_probe_timeout(i2c, DAS_I2C_TARGET_ADDRESS, 50u);
    if (result != DAS_OK) {
        g_das_i2c_test_evidence.error = UINT32_C(0x0c40) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    g_das_i2c_test_evidence.flags |= DAS_I2C_FLAG_PROBE;

    g_das_i2c_test_evidence.missing_probe_result =
        das_i2c_probe_timeout(i2c, DAS_I2C_MISSING_ADDRESS, 50u);
    if (g_das_i2c_test_evidence.missing_probe_result != DAS_ERROR_IO) {
        g_das_i2c_test_evidence.error = UINT32_C(0x0c50);
        for (;;) { __NOP(); }
    }
    g_das_i2c_test_evidence.flags |= DAS_I2C_FLAG_NACK;

    static const uint8_t write_pattern[] = {
        0x10u, 0x00u, 0xffu, 0x55u, 0xaau, 0x7eu, 0x81u, 0x11u,
        0x22u, 0x44u, 0x88u, 'D', 'A', 'S', 'I', '2'
    };
    target_clear_state();
    result = das_i2c_write_timeout(i2c,
                                   DAS_I2C_TARGET_ADDRESS,
                                   write_pattern,
                                   sizeof(write_pattern),
                                   50u);
    if (result != DAS_OK || !check_target_write(write_pattern, sizeof(write_pattern))) {
        g_das_i2c_test_evidence.error = UINT32_C(0x0c60) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    g_das_i2c_test_evidence.flags |= DAS_I2C_FLAG_WRITE;

    uint8_t read_data[16] = {0};
    g_target_register = 0u;
    result = das_i2c_read_timeout(i2c,
                                  DAS_I2C_TARGET_ADDRESS,
                                  read_data,
                                  sizeof(read_data),
                                  50u);
    if (result != DAS_OK || !check_target_read(read_data, sizeof(read_data), 0u)) {
        g_das_i2c_test_evidence.error = UINT32_C(0x0c70) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    g_das_i2c_test_evidence.flags |= DAS_I2C_FLAG_READ;

    const uint8_t selector = UINT8_C(7);
    uint8_t combined_data[12] = {0};
    target_clear_state();
    result = das_i2c_write_read_timeout(i2c,
                                        DAS_I2C_TARGET_ADDRESS,
                                        &selector,
                                        1u,
                                        combined_data,
                                        sizeof(combined_data),
                                        50u);
    if (result != DAS_OK || g_target_rx_count != 1u || g_target_rx[0] != selector ||
        !check_target_read(combined_data, sizeof(combined_data), selector)) {
        g_das_i2c_test_evidence.error = UINT32_C(0x0c80) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    ++g_das_i2c_test_evidence.bytes_checked; /* selector byte */
    g_das_i2c_test_evidence.flags |= DAS_I2C_FLAG_WRITE_READ;

    const das_i2c_config_t fast = {.frequency_hz = UINT32_C(400000)};
    result = das_board_i2c_init(DAS_BOARD_I2C_ARDUINO, &fast, &i2c);
    if (result != DAS_OK) {
        g_das_i2c_test_evidence.error = UINT32_C(0x0c90) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    target_update_timing(I2C1->TIMINGR);
    result = das_i2c_get_frequency(i2c, (uint32_t*)&g_das_i2c_test_evidence.fast_hz);
    if (result != DAS_OK || !frequency_close(g_das_i2c_test_evidence.fast_hz, fast.frequency_hz)) {
        g_das_i2c_test_evidence.error = UINT32_C(0x0ca0) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }

    const uint8_t fast_selector = UINT8_C(13);
    uint8_t fast_data[24] = {0};
    target_clear_state();
    result = das_i2c_write_read_timeout(i2c,
                                        DAS_I2C_TARGET_ADDRESS,
                                        &fast_selector,
                                        1u,
                                        fast_data,
                                        sizeof(fast_data),
                                        50u);
    if (result != DAS_OK || g_target_rx_count != 1u || g_target_rx[0] != fast_selector ||
        !check_target_read(fast_data, sizeof(fast_data), fast_selector)) {
        g_das_i2c_test_evidence.error = UINT32_C(0x0cb0) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    ++g_das_i2c_test_evidence.bytes_checked;
    g_das_i2c_test_evidence.flags |= DAS_I2C_FLAG_FAST;

    if (g_das_i2c_test_evidence.target_error_events != 0u ||
        g_das_i2c_test_evidence.flags != DAS_I2C_REQUIRED_FLAGS) {
        g_das_i2c_test_evidence.error = UINT32_C(0x0cc0);
        for (;;) { __NOP(); }
    }

    g_das_i2c_test_evidence.booted = 1u;
    for (;;) {
        ++g_das_i2c_test_evidence.heartbeat;
    }
}

void HardFault_Handler(void) {
    g_das_i2c_test_evidence.error = DAS_I2C_TEST_HARDFAULT;
    for (;;) { __NOP(); }
}
