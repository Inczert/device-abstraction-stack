// SPDX-License-Identifier: Apache-2.0

#include <das/board.h>
#include <das/board_resources.h>
#include <das/cortex_m/startup.h>
#include <das/gpio.h>
#include <das/irq.h>

#include "stm32h755xx.h"

#include <stdbool.h>
#include <stdint.h>

#define DAS_BUTTON_MAGIC UINT32_C(0x44415342)
#define DAS_BUTTON_ERROR_HARDFAULT UINT32_C(0xe0150001)

#define DAS_BUTTON_MAP_BUTTON  (UINT32_C(1) << 0u)
#define DAS_BUTTON_MAP_FIXTURE (UINT32_C(1) << 1u)
#define DAS_BUTTON_MAP_UART    (UINT32_C(1) << 2u)
#define DAS_BUTTON_MAP_I2C     (UINT32_C(1) << 3u)
#define DAS_BUTTON_MAP_SPI     (UINT32_C(1) << 4u)

typedef struct das_button_test_evidence {
    uint32_t magic;
    volatile uint32_t booted;
    volatile uint32_t error;
    volatile uint32_t heartbeat;
    volatile uint32_t map_flags;
    volatile uint32_t ready;
    volatile uint32_t pressed;
    volatile uint32_t irq_count;
    volatile uint32_t press_count;
    volatile uint32_t release_count;
    volatile uint32_t irq_priority_levels;
    volatile uint32_t irq_priority;
    /* Test-only physical wiring diagnostics. PC13 is the stock B1 route;
     * PA0 is the documented MB1363 solder-bridge alternative. */
    volatile uint32_t pc13_level;
    volatile uint32_t pa0_level;
    volatile uint32_t gpio_c_moder;
    volatile uint32_t gpio_c_pupdr;
    volatile uint32_t gpio_c_idr;
    volatile uint32_t gpio_a_idr;
    volatile uint32_t rcc_ahb4enr;
} das_button_test_evidence_t;

volatile das_button_test_evidence_t g_das_button_test_evidence = {
    .magic = DAS_BUTTON_MAGIC,
};

static const das_gpio_pin_t BUTTON_ALT_DIAGNOSTIC_PIN = {
    DAS_GPIO_PORT_A,
    0u,
};

static bool pin_equals(das_gpio_pin_t pin, das_gpio_port_t port, uint8_t number) {
    return pin.port == port && pin.pin == number;
}

static void validate_resource_map(void) {
    const das_gpio_pin_t button = das_board_button_pin(DAS_BOARD_BUTTON_USER);
    if (pin_equals(button, DAS_GPIO_PORT_C, 13u)) {
        g_das_button_test_evidence.map_flags |= DAS_BUTTON_MAP_BUTTON;
    }

    const das_gpio_pin_t d3 = das_board_gpio_pin(DAS_BOARD_GPIO_ARDUINO_D3);
    const das_gpio_pin_t d4 = das_board_gpio_pin(DAS_BOARD_GPIO_ARDUINO_D4);
    if (pin_equals(d3, DAS_GPIO_PORT_E, 13u) &&
        pin_equals(d4, DAS_GPIO_PORT_E, 14u)) {
        g_das_button_test_evidence.map_flags |= DAS_BUTTON_MAP_FIXTURE;
    }

    das_board_uart_pins_t vcp = {0};
    das_board_uart_pins_t arduino_uart = {0};
    if (das_board_uart_get_pins(DAS_BOARD_UART_STLINK_VCP, &vcp) == DAS_OK &&
        das_board_uart_get_pins(DAS_BOARD_UART_ARDUINO, &arduino_uart) == DAS_OK &&
        pin_equals(vcp.tx, DAS_GPIO_PORT_D, 8u) &&
        pin_equals(vcp.rx, DAS_GPIO_PORT_D, 9u) &&
        pin_equals(arduino_uart.tx, DAS_GPIO_PORT_B, 6u) &&
        pin_equals(arduino_uart.rx, DAS_GPIO_PORT_B, 7u)) {
        g_das_button_test_evidence.map_flags |= DAS_BUTTON_MAP_UART;
    }

    das_board_i2c_pins_t i2c = {0};
    if (das_board_i2c_get_pins(DAS_BOARD_I2C_ARDUINO, &i2c) == DAS_OK &&
        pin_equals(i2c.scl, DAS_GPIO_PORT_B, 8u) &&
        pin_equals(i2c.sda, DAS_GPIO_PORT_B, 9u)) {
        g_das_button_test_evidence.map_flags |= DAS_BUTTON_MAP_I2C;
    }

    das_board_spi_pins_t spi = {0};
    if (das_board_spi_get_pins(DAS_BOARD_SPI_ARDUINO, &spi) == DAS_OK &&
        pin_equals(spi.sck, DAS_GPIO_PORT_A, 5u) &&
        pin_equals(spi.miso, DAS_GPIO_PORT_A, 6u) &&
        pin_equals(spi.mosi, DAS_GPIO_PORT_B, 5u) &&
        pin_equals(spi.cs, DAS_GPIO_PORT_D, 14u)) {
        g_das_button_test_evidence.map_flags |= DAS_BUTTON_MAP_SPI;
    }
}

static void capture_button_diagnostics(void) {
    const das_gpio_pin_t button = das_board_button_pin(DAS_BOARD_BUTTON_USER);
    g_das_button_test_evidence.pc13_level =
        das_gpio_read_input(button) ? 1u : 0u;
    g_das_button_test_evidence.pa0_level =
        das_gpio_read_input(BUTTON_ALT_DIAGNOSTIC_PIN) ? 1u : 0u;
    g_das_button_test_evidence.gpio_c_moder = GPIOC->MODER;
    g_das_button_test_evidence.gpio_c_pupdr = GPIOC->PUPDR;
    g_das_button_test_evidence.gpio_c_idr = GPIOC->IDR;
    g_das_button_test_evidence.gpio_a_idr = GPIOA->IDR;
    g_das_button_test_evidence.rcc_ahb4enr = RCC->AHB4ENR;
}

int main(void) {
    validate_resource_map();

    /* This is diagnostic-only. MB1363 can route B1 to PA0 instead of PC13.
     * Pull PA0 down so an unconnected alternate route has a deterministic
     * level while still allowing a button-routed PA0 to rise when pressed. */
    if (das_gpio_input_init(BUTTON_ALT_DIAGNOSTIC_PIN, DAS_GPIO_PULL_DOWN) != DAS_OK) {
        g_das_button_test_evidence.error = UINT32_C(0x1500);
        for (;;) { __NOP(); }
    }

    das_irq_t irq = DAS_IRQ_INVALID;
    if (das_board_button_interrupt_get_irq(DAS_BOARD_BUTTON_USER, &irq) != DAS_OK ||
        !das_irq_is_valid(irq) ||
        das_irq_disable(irq) != DAS_OK ||
        das_irq_clear_pending(irq) != DAS_OK ||
        das_board_button_interrupt_configure(
            DAS_BOARD_BUTTON_USER,
            DAS_BOARD_BUTTON_EVENT_BOTH) != DAS_OK ||
        das_board_button_interrupt_clear(DAS_BOARD_BUTTON_USER) != DAS_OK) {
        g_das_button_test_evidence.error = UINT32_C(0x1501);
        for (;;) { __NOP(); }
    }

    g_das_button_test_evidence.irq_priority_levels = das_irq_priority_levels();
    const uint32_t priority = g_das_button_test_evidence.irq_priority_levels > 1u
        ? g_das_button_test_evidence.irq_priority_levels / 2u
        : 0u;
    uint32_t observed_priority = UINT32_MAX;

    if (das_irq_set_priority(irq, priority) != DAS_OK ||
        das_irq_get_priority(irq, &observed_priority) != DAS_OK ||
        observed_priority != priority ||
        das_irq_enable(irq) != DAS_OK ||
        das_board_button_interrupt_enable(DAS_BOARD_BUTTON_USER, true) != DAS_OK) {
        g_das_button_test_evidence.error = UINT32_C(0x1502);
        for (;;) { __NOP(); }
    }
    g_das_button_test_evidence.irq_priority = observed_priority;

    g_das_button_test_evidence.pressed =
        das_board_button_is_pressed(DAS_BOARD_BUTTON_USER) ? 1u : 0u;
    capture_button_diagnostics();
    g_das_button_test_evidence.ready = 1u;
    g_das_button_test_evidence.booted = 1u;

    for (;;) {
        ++g_das_button_test_evidence.heartbeat;
        g_das_button_test_evidence.pressed =
            das_board_button_is_pressed(DAS_BOARD_BUTTON_USER) ? 1u : 0u;
        capture_button_diagnostics();
    }
}

void HardFault_Handler(void) {
    g_das_button_test_evidence.error = DAS_BUTTON_ERROR_HARDFAULT;
    for (;;) { __NOP(); }
}

void EXTI15_10_IRQHandler(void) {
    if (!das_board_button_interrupt_pending(DAS_BOARD_BUTTON_USER)) {
        return;
    }

    const bool pressed = das_board_button_is_pressed(DAS_BOARD_BUTTON_USER);
    (void)das_board_button_interrupt_clear(DAS_BOARD_BUTTON_USER);
    ++g_das_button_test_evidence.irq_count;
    if (pressed) {
        ++g_das_button_test_evidence.press_count;
    } else {
        ++g_das_button_test_evidence.release_count;
    }
}
