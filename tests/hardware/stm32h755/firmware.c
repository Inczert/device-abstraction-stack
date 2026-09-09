// SPDX-License-Identifier: Apache-2.0

#include <das/board.h>

#include "stm32h755xx.h"

#include <stdint.h>

#define DAS_HW_MAGIC UINT32_C(0x44415331)
#define DAS_HW_ERROR_HARDFAULT UINT32_C(0xe0030001)

extern uint32_t __StackTop;
extern uint32_t __data_load__;
extern uint32_t __data_start__;
extern uint32_t __data_end__;
extern uint32_t __bss_start__;
extern uint32_t __bss_end__;

void Reset_Handler(void);
void HardFault_Handler(void);

__attribute__((section(".isr_vector"), used, aligned(256)))
const uintptr_t g_das_vector_table[16] = {
    (uintptr_t)&__StackTop,
    (uintptr_t)&Reset_Handler,
    0u,
    (uintptr_t)&HardFault_Handler,
    0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u,
};

typedef enum das_hw_command {
    DAS_HW_COMMAND_IDLE = 0,
    DAS_HW_COMMAND_ALL_OFF = 1,
    DAS_HW_COMMAND_GREEN_ONLY = 2,
    DAS_HW_COMMAND_YELLOW_ONLY = 3,
    DAS_HW_COMMAND_RED_ONLY = 4,
    DAS_HW_COMMAND_ALL_BLINK = 5
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
} das_hw_evidence_t;

volatile uint32_t g_das_hw_command = DAS_HW_COMMAND_IDLE;
volatile das_hw_evidence_t g_das_hw_evidence = {
    DAS_HW_MAGIC, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u
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
