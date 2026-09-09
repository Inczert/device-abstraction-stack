// SPDX-License-Identifier: Apache-2.0

#include <das/board.h>
#include <das/board_resources.h>

#include "uart_internal.h"

#include <stdint.h>

#define DAS_NUCLEO_UART_AF UINT8_C(7)

static das_gpio_pin_t invalid_pin(void) {
    return (das_gpio_pin_t){DAS_GPIO_PORT_A, UINT8_C(0xff)};
}

static bool button_valid(das_board_button_t button) {
    return (unsigned)button < (unsigned)DAS_BOARD_BUTTON_COUNT;
}

static bool uart_valid(das_board_uart_resource_t resource) {
    return (unsigned)resource < (unsigned)DAS_BOARD_UART_COUNT;
}

static bool i2c_valid(das_board_i2c_resource_t resource) {
    return (unsigned)resource < (unsigned)DAS_BOARD_I2C_COUNT;
}

static bool spi_valid(das_board_spi_resource_t resource) {
    return (unsigned)resource < (unsigned)DAS_BOARD_SPI_COUNT;
}

static bool gpio_resource_valid(das_board_gpio_resource_t resource) {
    return (unsigned)resource < (unsigned)DAS_BOARD_GPIO_COUNT;
}

static const das_gpio_pin_t BUTTON_PINS[DAS_BOARD_BUTTON_COUNT] = {
    [DAS_BOARD_BUTTON_USER] = {DAS_GPIO_PORT_C, 13u},
};

static const das_board_uart_pins_t UART_PINS[DAS_BOARD_UART_COUNT] = {
    [DAS_BOARD_UART_STLINK_VCP] = {
        .tx = {DAS_GPIO_PORT_D, 8u},
        .rx = {DAS_GPIO_PORT_D, 9u},
    },
    [DAS_BOARD_UART_ARDUINO] = {
        .tx = {DAS_GPIO_PORT_B, 6u},
        .rx = {DAS_GPIO_PORT_B, 7u},
    },
};

static const stm32h755_uart_instance_t UART_INSTANCES[DAS_BOARD_UART_COUNT] = {
    [DAS_BOARD_UART_STLINK_VCP] = STM32H755_UART_USART3,
    [DAS_BOARD_UART_ARDUINO] = STM32H755_UART_USART1,
};

static const das_board_i2c_pins_t I2C_PINS[DAS_BOARD_I2C_COUNT] = {
    [DAS_BOARD_I2C_ARDUINO] = {
        .scl = {DAS_GPIO_PORT_B, 8u},
        .sda = {DAS_GPIO_PORT_B, 9u},
    },
};

static const das_board_spi_pins_t SPI_PINS[DAS_BOARD_SPI_COUNT] = {
    [DAS_BOARD_SPI_ARDUINO] = {
        .sck = {DAS_GPIO_PORT_A, 5u},
        .miso = {DAS_GPIO_PORT_A, 6u},
        .mosi = {DAS_GPIO_PORT_B, 5u},
        .cs = {DAS_GPIO_PORT_D, 14u},
    },
};

static const das_gpio_pin_t GPIO_RESOURCE_PINS[DAS_BOARD_GPIO_COUNT] = {
    [DAS_BOARD_GPIO_ARDUINO_D3] = {DAS_GPIO_PORT_E, 13u},
    [DAS_BOARD_GPIO_ARDUINO_D4] = {DAS_GPIO_PORT_E, 14u},
};

das_result_t das_board_uart_get_pins(das_board_uart_resource_t resource,
                                     das_board_uart_pins_t* pins) {
    if (!uart_valid(resource) || pins == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    *pins = UART_PINS[resource];
    return DAS_OK;
}

das_result_t das_board_uart_init(das_board_uart_resource_t resource,
                                 const das_uart_config_t* config,
                                 das_uart_t* uart) {
    if (!uart_valid(resource) || config == 0 || uart == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    *uart = DAS_UART_INVALID;
    const das_board_uart_pins_t pins = UART_PINS[resource];
    const das_gpio_config_t tx_config = {
        .mode = DAS_GPIO_MODE_ALTERNATE,
        .pull = DAS_GPIO_PULL_NONE,
        .output_type = DAS_GPIO_OUTPUT_PUSH_PULL,
        .speed = DAS_GPIO_SPEED_HIGH,
        .alternate = DAS_NUCLEO_UART_AF,
        .initial_high = true,
    };
    const das_gpio_config_t rx_config = {
        .mode = DAS_GPIO_MODE_ALTERNATE,
        .pull = DAS_GPIO_PULL_UP,
        .output_type = DAS_GPIO_OUTPUT_PUSH_PULL,
        .speed = DAS_GPIO_SPEED_HIGH,
        .alternate = DAS_NUCLEO_UART_AF,
        .initial_high = true,
    };

    das_result_t result = das_gpio_configure(pins.tx, &tx_config);
    if (result != DAS_OK) {
        return result;
    }
    result = das_gpio_configure(pins.rx, &rx_config);
    if (result != DAS_OK) {
        return result;
    }

    const das_uart_t resolved = stm32h755_uart_handle(UART_INSTANCES[resource]);
    if (!das_uart_is_valid(resolved)) {
        return DAS_ERROR_UNSUPPORTED;
    }

    result = das_uart_init(resolved, config);
    if (result != DAS_OK) {
        return result;
    }

    *uart = resolved;
    return DAS_OK;
}

das_result_t das_board_i2c_get_pins(das_board_i2c_resource_t resource,
                                    das_board_i2c_pins_t* pins) {
    if (!i2c_valid(resource) || pins == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    *pins = I2C_PINS[resource];
    return DAS_OK;
}

das_result_t das_board_spi_get_pins(das_board_spi_resource_t resource,
                                    das_board_spi_pins_t* pins) {
    if (!spi_valid(resource) || pins == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    *pins = SPI_PINS[resource];
    return DAS_OK;
}

das_gpio_pin_t das_board_gpio_pin(das_board_gpio_resource_t resource) {
    if (!gpio_resource_valid(resource)) {
        return invalid_pin();
    }
    return GPIO_RESOURCE_PINS[resource];
}

das_gpio_pin_t das_board_button_pin(das_board_button_t button) {
    if (!button_valid(button)) {
        return invalid_pin();
    }
    return BUTTON_PINS[button];
}

das_result_t das_board_button_init(das_board_button_t button) {
    if (!button_valid(button)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    /* B1 has a board-level pull-down resistor; do not add a second bias here. */
    return das_gpio_input_init(BUTTON_PINS[button], DAS_GPIO_PULL_NONE);
}

bool das_board_button_is_pressed(das_board_button_t button) {
    return button_valid(button) && das_gpio_read_input(BUTTON_PINS[button]);
}

das_result_t das_board_button_interrupt_configure(
    das_board_button_t button,
    das_board_button_event_t event) {
    if (!button_valid(button)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    das_gpio_interrupt_edge_t edge;
    switch (event) {
        case DAS_BOARD_BUTTON_EVENT_PRESS:
            edge = DAS_GPIO_INTERRUPT_RISING;
            break;
        case DAS_BOARD_BUTTON_EVENT_RELEASE:
            edge = DAS_GPIO_INTERRUPT_FALLING;
            break;
        case DAS_BOARD_BUTTON_EVENT_BOTH:
            edge = DAS_GPIO_INTERRUPT_BOTH;
            break;
        default:
            return DAS_ERROR_INVALID_ARGUMENT;
    }

    const das_result_t init_result = das_board_button_init(button);
    if (init_result != DAS_OK) {
        return init_result;
    }
    return das_gpio_interrupt_configure(BUTTON_PINS[button], edge);
}

das_result_t das_board_button_interrupt_enable(das_board_button_t button,
                                               bool enabled) {
    if (!button_valid(button)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    return das_gpio_interrupt_enable(BUTTON_PINS[button], enabled);
}

bool das_board_button_interrupt_pending(das_board_button_t button) {
    return button_valid(button) && das_gpio_interrupt_pending(BUTTON_PINS[button]);
}

das_result_t das_board_button_interrupt_clear(das_board_button_t button) {
    if (!button_valid(button)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    return das_gpio_interrupt_clear(BUTTON_PINS[button]);
}

das_result_t das_board_button_interrupt_get_irq(das_board_button_t button,
                                                das_irq_t* irq) {
    if (!button_valid(button) || irq == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    return das_gpio_interrupt_get_irq(BUTTON_PINS[button], irq);
}
