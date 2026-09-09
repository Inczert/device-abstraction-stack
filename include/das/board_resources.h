// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_BOARD_RESOURCES_H
#define DAS_BOARD_RESOURCES_H

#include <das/gpio.h>
#include <das/result.h>

/** Semantic UART-style board connections. */
typedef enum das_board_uart_resource {
    DAS_BOARD_UART_STLINK_VCP = 0,
    DAS_BOARD_UART_ARDUINO,
    DAS_BOARD_UART_COUNT
} das_board_uart_resource_t;

typedef struct das_board_uart_pins {
    das_gpio_pin_t tx;
    das_gpio_pin_t rx;
} das_board_uart_pins_t;

/** Resolve the pins associated with a semantic UART board connection. */
das_result_t das_board_uart_get_pins(das_board_uart_resource_t resource,
                                     das_board_uart_pins_t* pins);

/** Semantic I2C board connections. */
typedef enum das_board_i2c_resource {
    DAS_BOARD_I2C_ARDUINO = 0,
    DAS_BOARD_I2C_COUNT
} das_board_i2c_resource_t;

typedef struct das_board_i2c_pins {
    das_gpio_pin_t scl;
    das_gpio_pin_t sda;
} das_board_i2c_pins_t;

/** Resolve the pins associated with a semantic I2C board connection. */
das_result_t das_board_i2c_get_pins(das_board_i2c_resource_t resource,
                                    das_board_i2c_pins_t* pins);

/** Semantic SPI board connections. */
typedef enum das_board_spi_resource {
    DAS_BOARD_SPI_ARDUINO = 0,
    DAS_BOARD_SPI_COUNT
} das_board_spi_resource_t;

typedef struct das_board_spi_pins {
    das_gpio_pin_t sck;
    das_gpio_pin_t miso;
    das_gpio_pin_t mosi;
    /** Board-level chip-select GPIO associated with the connector. */
    das_gpio_pin_t cs;
} das_board_spi_pins_t;

/** Resolve the pins associated with a semantic SPI board connection. */
das_result_t das_board_spi_get_pins(das_board_spi_resource_t resource,
                                    das_board_spi_pins_t* pins);

/** Small set of connector GPIO aliases used directly by DAS applications/tests. */
typedef enum das_board_gpio_resource {
    DAS_BOARD_GPIO_ARDUINO_D3 = 0,
    DAS_BOARD_GPIO_ARDUINO_D4,
    DAS_BOARD_GPIO_COUNT
} das_board_gpio_resource_t;

/** Resolve a semantic connector GPIO to a generic DAS GPIO pin. */
das_gpio_pin_t das_board_gpio_pin(das_board_gpio_resource_t resource);

#endif
