// SPDX-License-Identifier: Apache-2.0

#include <das/board.h>
#include <das/gpio.h>

#include "stm32h755xx.h"

#include <stdint.h>

#define DAS_HW_MAGIC UINT32_C(0x44415331)
#define DAS_HW_ERROR_HARDFAULT UINT32_C(0xe0030001)

#define DAS_GPIO_FLAG_LOOPBACK_LOW       (UINT32_C(1) << 0u)
#define DAS_GPIO_FLAG_LOOPBACK_HIGH      (UINT32_C(1) << 1u)
#define DAS_GPIO_FLAG_PULL_UP_HIGH       (UINT32_C(1) << 2u)
#define DAS_GPIO_FLAG_PULL_DOWN_LOW      (UINT32_C(1) << 3u)
#define DAS_GPIO_FLAG_OPEN_DRAIN_LOW     (UINT32_C(1) << 4u)
#define DAS_GPIO_FLAG_OPEN_DRAIN_RELEASE (UINT32_C(1) << 5u)
#define DAS_GPIO_FLAG_EXTI_RISING        (UINT32_C(1) << 6u)
#define DAS_GPIO_FLAG_EXTI_FALLING       (UINT32_C(1) << 7u)

/* Arduino D4 -> D3 jumper for GPIO electrical-path qualification. */
static const das_gpio_pin_t GPIO_TEST_OUTPUT = {DAS_GPIO_PORT_E, 14u}; /* D4 */
static const das_gpio_pin_t GPIO_TEST_INPUT = {DAS_GPIO_PORT_E, 13u};  /* D3 */

extern uint32_t __StackTop;
extern uint32_t __data_load__;
extern uint32_t __data_start__;
extern uint32_t __data_end__;
extern uint32_t __bss_start__;
extern uint32_t __bss_end__;

void Reset_Handler(void);
void HardFault_Handler(void);
void EXTI15_10_IRQHandler(void);

#define DAS_VECTOR_EXTI15_10 (16 + EXTI15_10_IRQn)

__attribute__((section(".isr_vector"), used, aligned(256)))
const uintptr_t g_das_vector_table[DAS_VECTOR_EXTI15_10 + 1] = {
    [0] = (uintptr_t)&__StackTop,
    [1] = (uintptr_t)&Reset_Handler,
    [3] = (uintptr_t)&HardFault_Handler,
    [DAS_VECTOR_EXTI15_10] = (uintptr_t)&EXTI15_10_IRQHandler,
};

typedef enum das_hw_command {
    DAS_HW_COMMAND_IDLE = 0,
    DAS_HW_COMMAND_ALL_OFF = 1,
    DAS_HW_COMMAND_GREEN_ONLY = 2,
    DAS_HW_COMMAND_YELLOW_ONLY = 3,
    DAS_HW_COMMAND_RED_ONLY = 4,
    DAS_HW_COMMAND_ALL_BLINK = 5,
    DAS_HW_COMMAND_GPIO_LOOPBACK = 6,
    DAS_HW_COMMAND_GPIO_PULL_UP = 7,
    DAS_HW_COMMAND_GPIO_PULL_DOWN = 8,
    DAS_HW_COMMAND_GPIO_OPEN_DRAIN = 9,
    DAS_HW_COMMAND_GPIO_EXTI = 10
} das_hw_command_t;

typedef struct das_hw_evidence {
    uint32_t magic;
    volatile uint32_t booted;
    volatile uint32_t error;
    volatile uint32_t heartbeat;
    volatile uint32_t command_seen;
    volatile uint32_t led_mask;
    volatile uint32_t blink_count;
    volatile uint32_t gpio_b_moder;
    volatile uint32_t gpio_e_moder;
    volatile uint32_t gpio_b_odr;
    volatile uint32_t gpio_e_odr;
    volatile uint32_t rcc_ahb4enr;
    volatile uint32_t gpio_test_flags;
    volatile uint32_t gpio_input_low;
    volatile uint32_t gpio_input_high;
    volatile uint32_t exti_irq_count;
    volatile uint32_t exti_rising_count;
    volatile uint32_t exti_falling_count;
} das_hw_evidence_t;

volatile uint32_t g_das_hw_command = DAS_HW_COMMAND_IDLE;
volatile das_hw_evidence_t g_das_hw_evidence = {
    .magic = DAS_HW_MAGIC,
};

static void memory_init(void) {
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

static void short_delay(void) {
    for (volatile uint32_t i = 0u; i < UINT32_C(20000); ++i) {
        __NOP();
    }
}

static uint32_t current_led_mask(void) {
    uint32_t mask = 0u;
    if (das_board_led_is_on(DAS_BOARD_LED_GREEN)) mask |= UINT32_C(1) << 0u;
    if (das_board_led_is_on(DAS_BOARD_LED_YELLOW)) mask |= UINT32_C(1) << 1u;
    if (das_board_led_is_on(DAS_BOARD_LED_RED)) mask |= UINT32_C(1) << 2u;
    return mask;
}

static void capture_registers(void) {
    g_das_hw_evidence.led_mask = current_led_mask();
    g_das_hw_evidence.gpio_b_moder = GPIOB->MODER;
    g_das_hw_evidence.gpio_e_moder = GPIOE->MODER;
    g_das_hw_evidence.gpio_b_odr = GPIOB->ODR;
    g_das_hw_evidence.gpio_e_odr = GPIOE->ODR;
    g_das_hw_evidence.rcc_ahb4enr = RCC->AHB4ENR;
}

static void set_led_mask(uint32_t mask) {
    (void)das_board_led_set(DAS_BOARD_LED_GREEN, (mask & UINT32_C(1)) != 0u);
    (void)das_board_led_set(DAS_BOARD_LED_YELLOW, (mask & UINT32_C(2)) != 0u);
    (void)das_board_led_set(DAS_BOARD_LED_RED, (mask & UINT32_C(4)) != 0u);
    capture_registers();
}

static void apply_static_command(uint32_t command) {
    switch (command) {
        case DAS_HW_COMMAND_ALL_OFF: set_led_mask(0u); break;
        case DAS_HW_COMMAND_GREEN_ONLY: set_led_mask(UINT32_C(1)); break;
        case DAS_HW_COMMAND_YELLOW_ONLY: set_led_mask(UINT32_C(2)); break;
        case DAS_HW_COMMAND_RED_ONLY: set_led_mask(UINT32_C(4)); break;
        default: break;
    }
}

static void reset_gpio_test_evidence(void) {
    g_das_hw_evidence.gpio_test_flags = 0u;
    g_das_hw_evidence.gpio_input_low = UINT32_MAX;
    g_das_hw_evidence.gpio_input_high = UINT32_MAX;
    g_das_hw_evidence.exti_irq_count = 0u;
    g_das_hw_evidence.exti_rising_count = 0u;
    g_das_hw_evidence.exti_falling_count = 0u;
}

static void run_gpio_loopback(void) {
    reset_gpio_test_evidence();
    if (das_gpio_input_init(GPIO_TEST_INPUT, DAS_GPIO_PULL_NONE) != DAS_OK ||
        das_gpio_output_init(GPIO_TEST_OUTPUT, false) != DAS_OK) {
        g_das_hw_evidence.error = UINT32_C(0x2001);
        return;
    }

    (void)das_gpio_write(GPIO_TEST_OUTPUT, false);
    short_delay();
    g_das_hw_evidence.gpio_input_low = das_gpio_read_input(GPIO_TEST_INPUT) ? 1u : 0u;
    if (g_das_hw_evidence.gpio_input_low == 0u) {
        g_das_hw_evidence.gpio_test_flags |= DAS_GPIO_FLAG_LOOPBACK_LOW;
    }

    (void)das_gpio_write(GPIO_TEST_OUTPUT, true);
    short_delay();
    g_das_hw_evidence.gpio_input_high = das_gpio_read_input(GPIO_TEST_INPUT) ? 1u : 0u;
    if (g_das_hw_evidence.gpio_input_high == 1u) {
        g_das_hw_evidence.gpio_test_flags |= DAS_GPIO_FLAG_LOOPBACK_HIGH;
    }

    (void)das_gpio_write(GPIO_TEST_OUTPUT, false);
}

static void run_gpio_pull_up(void) {
    reset_gpio_test_evidence();
    if (das_gpio_input_init(GPIO_TEST_INPUT, DAS_GPIO_PULL_UP) != DAS_OK) {
        g_das_hw_evidence.error = UINT32_C(0x2002);
        return;
    }
    short_delay();
    g_das_hw_evidence.gpio_input_high = das_gpio_read_input(GPIO_TEST_INPUT) ? 1u : 0u;
    if (g_das_hw_evidence.gpio_input_high == 1u) {
        g_das_hw_evidence.gpio_test_flags |= DAS_GPIO_FLAG_PULL_UP_HIGH;
    }
}

static void run_gpio_pull_down(void) {
    reset_gpio_test_evidence();
    if (das_gpio_input_init(GPIO_TEST_INPUT, DAS_GPIO_PULL_DOWN) != DAS_OK) {
        g_das_hw_evidence.error = UINT32_C(0x2003);
        return;
    }
    short_delay();
    g_das_hw_evidence.gpio_input_low = das_gpio_read_input(GPIO_TEST_INPUT) ? 1u : 0u;
    if (g_das_hw_evidence.gpio_input_low == 0u) {
        g_das_hw_evidence.gpio_test_flags |= DAS_GPIO_FLAG_PULL_DOWN_LOW;
    }
}

static void run_gpio_open_drain(void) {
    reset_gpio_test_evidence();
    if (das_gpio_input_init(GPIO_TEST_INPUT, DAS_GPIO_PULL_UP) != DAS_OK ||
        das_gpio_output_init_ex(GPIO_TEST_OUTPUT,
                                DAS_GPIO_OUTPUT_OPEN_DRAIN,
                                DAS_GPIO_PULL_NONE,
                                DAS_GPIO_SPEED_LOW,
                                true) != DAS_OK) {
        g_das_hw_evidence.error = UINT32_C(0x2004);
        return;
    }

    (void)das_gpio_write(GPIO_TEST_OUTPUT, false);
    short_delay();
    g_das_hw_evidence.gpio_input_low = das_gpio_read_input(GPIO_TEST_INPUT) ? 1u : 0u;
    if (g_das_hw_evidence.gpio_input_low == 0u) {
        g_das_hw_evidence.gpio_test_flags |= DAS_GPIO_FLAG_OPEN_DRAIN_LOW;
    }

    (void)das_gpio_write(GPIO_TEST_OUTPUT, true);
    short_delay();
    g_das_hw_evidence.gpio_input_high = das_gpio_read_input(GPIO_TEST_INPUT) ? 1u : 0u;
    if (g_das_hw_evidence.gpio_input_high == 1u) {
        g_das_hw_evidence.gpio_test_flags |= DAS_GPIO_FLAG_OPEN_DRAIN_RELEASE;
    }
}

static void run_gpio_exti(void) {
    reset_gpio_test_evidence();
    NVIC_DisableIRQ(EXTI15_10_IRQn);

    if (das_gpio_input_init(GPIO_TEST_INPUT, DAS_GPIO_PULL_NONE) != DAS_OK ||
        das_gpio_output_init(GPIO_TEST_OUTPUT, false) != DAS_OK ||
        das_gpio_interrupt_configure(GPIO_TEST_INPUT, DAS_GPIO_INTERRUPT_BOTH) != DAS_OK ||
        das_gpio_interrupt_clear(GPIO_TEST_INPUT) != DAS_OK) {
        g_das_hw_evidence.error = UINT32_C(0x2005);
        return;
    }

    short_delay();
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
    (void)das_gpio_interrupt_enable(GPIO_TEST_INPUT, true);

    for (uint32_t cycle = 0u; cycle < 2u; ++cycle) {
        (void)das_gpio_write(GPIO_TEST_OUTPUT, true);
        short_delay();
        (void)das_gpio_write(GPIO_TEST_OUTPUT, false);
        short_delay();
    }

    (void)das_gpio_interrupt_enable(GPIO_TEST_INPUT, false);
    NVIC_DisableIRQ(EXTI15_10_IRQn);

    if (g_das_hw_evidence.exti_rising_count >= 2u) {
        g_das_hw_evidence.gpio_test_flags |= DAS_GPIO_FLAG_EXTI_RISING;
    }
    if (g_das_hw_evidence.exti_falling_count >= 2u) {
        g_das_hw_evidence.gpio_test_flags |= DAS_GPIO_FLAG_EXTI_FALLING;
    }

    /* Return both fixture pins to high impedance before handing control back. */
    (void)das_gpio_input_init(GPIO_TEST_OUTPUT, DAS_GPIO_PULL_NONE);
    (void)das_gpio_input_init(GPIO_TEST_INPUT, DAS_GPIO_PULL_NONE);
}

static void apply_gpio_command(uint32_t command) {
    switch (command) {
        case DAS_HW_COMMAND_GPIO_LOOPBACK: run_gpio_loopback(); break;
        case DAS_HW_COMMAND_GPIO_PULL_UP: run_gpio_pull_up(); break;
        case DAS_HW_COMMAND_GPIO_PULL_DOWN: run_gpio_pull_down(); break;
        case DAS_HW_COMMAND_GPIO_OPEN_DRAIN: run_gpio_open_drain(); break;
        case DAS_HW_COMMAND_GPIO_EXTI: run_gpio_exti(); break;
        default: break;
    }
    capture_registers();
}

static void firmware_main(void) {
    if (das_board_led_init_all(false) != DAS_OK) {
        g_das_hw_evidence.error = UINT32_C(0x1001);
        for (;;) { __NOP(); }
    }

    SysTick->LOAD = UINT32_C(0x00ffffff);
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    uint32_t last_command = UINT32_MAX;
    uint32_t blink_ticks = 0u;
    g_das_hw_evidence.booted = 1u;
    capture_registers();

    for (;;) {
        ++g_das_hw_evidence.heartbeat;
        const uint32_t command = g_das_hw_command;

        if (command != last_command) {
            last_command = command;
            g_das_hw_evidence.command_seen = command;
            blink_ticks = 0u;
            if (command == DAS_HW_COMMAND_ALL_BLINK) {
                set_led_mask(0u);
            } else if (command >= DAS_HW_COMMAND_GPIO_LOOPBACK) {
                apply_gpio_command(command);
            } else {
                apply_static_command(command);
            }
        }

        if (command == DAS_HW_COMMAND_ALL_BLINK &&
            (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0u) {
            if (++blink_ticks >= 2u) {
                blink_ticks = 0u;
                (void)das_board_led_toggle(DAS_BOARD_LED_GREEN);
                (void)das_board_led_toggle(DAS_BOARD_LED_YELLOW);
                (void)das_board_led_toggle(DAS_BOARD_LED_RED);
                ++g_das_hw_evidence.blink_count;
                capture_registers();
            }
        } else if ((g_das_hw_evidence.heartbeat & UINT32_C(0x3fff)) == 0u) {
            capture_registers();
        }
    }
}

void Reset_Handler(void) {
    memory_init();
    SCB->VTOR = (uint32_t)(uintptr_t)g_das_vector_table;
    __DSB();
    __ISB();
    firmware_main();
    for (;;) { __NOP(); }
}

void HardFault_Handler(void) {
    g_das_hw_evidence.error = DAS_HW_ERROR_HARDFAULT;
    capture_registers();
    for (;;) { __NOP(); }
}

void EXTI15_10_IRQHandler(void) {
    if (das_gpio_interrupt_pending(GPIO_TEST_INPUT)) {
        const bool high = das_gpio_read_input(GPIO_TEST_INPUT);
        (void)das_gpio_interrupt_clear(GPIO_TEST_INPUT);
        ++g_das_hw_evidence.exti_irq_count;
        if (high) {
            ++g_das_hw_evidence.exti_rising_count;
        } else {
            ++g_das_hw_evidence.exti_falling_count;
        }
    }
}
