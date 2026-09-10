// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_BOARD_RESOURCES_H
#define DAS_BOARD_RESOURCES_H

#include <das/gpio.h>
#include <das/i2c.h>
#include <das/result.h>
#include <das/spi.h>
#include <das/timer.h>
#include <das/uart.h>

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

das_result_t das_board_uart_get_pins(das_board_uart_resource_t resource,
                                     das_board_uart_pins_t* pins);
das_result_t das_board_uart_init(das_board_uart_resource_t resource,
                                 const das_uart_config_t* config,
                                 das_uart_t* uart);

/** Semantic PWM-capable board outputs. */
typedef enum das_board_pwm_resource {
    DAS_BOARD_PWM_ARDUINO_D4 = 0,
    DAS_BOARD_PWM_COUNT
} das_board_pwm_resource_t;

das_gpio_pin_t das_board_pwm_pin(das_board_pwm_resource_t resource);
das_result_t das_board_pwm_init(das_board_pwm_resource_t resource,
                                const das_pwm_config_t* config,
                                das_pwm_t* pwm);

/** Semantic I2C board connections. */
typedef enum das_board_i2c_resource {
    DAS_BOARD_I2C_ARDUINO = 0,
    DAS_BOARD_I2C_COUNT
} das_board_i2c_resource_t;

typedef struct das_board_i2c_pins {
    das_gpio_pin_t scl;
    das_gpio_pin_t sda;
} das_board_i2c_pins_t;

das_result_t das_board_i2c_get_pins(das_board_i2c_resource_t resource,
                                    das_board_i2c_pins_t* pins);

/** Configure the board I2C pins and initialize the generic controller. */
das_result_t das_board_i2c_init(das_board_i2c_resource_t resource,
                                const das_i2c_config_t* config,
                                das_i2c_t* i2c);

/** Semantic SPI board connections. */
typedef enum das_board_spi_resource {
    DAS_BOARD_SPI_ARDUINO = 0,
    DAS_BOARD_SPI_COUNT
} das_board_spi_resource_t;

typedef struct das_board_spi_pins {
    das_gpio_pin_t sck;
    das_gpio_pin_t miso;
    das_gpio_pin_t mosi;
    /** Board-level active-low chip-select GPIO associated with the connector. */
    das_gpio_pin_t cs;
} das_board_spi_pins_t;

das_result_t das_board_spi_get_pins(das_board_spi_resource_t resource,
                                    das_board_spi_pins_t* pins);

/** Configure the board SPI signals/default CS and initialize the controller. */
das_result_t das_board_spi_init(das_board_spi_resource_t resource,
                                const das_spi_config_t* config,
                                das_spi_t* spi);

/**
 * Drive the board resource's default active-low chip-select.
 *
 * SPI transfers never change chip-select implicitly. Applications with several
 * devices may instead use arbitrary DAS GPIOs as their chip-select lines.
 */
das_result_t das_board_spi_chip_select(das_board_spi_resource_t resource,
                                       bool selected);

/** Small set of connector GPIO aliases used directly by DAS applications/tests. */
typedef enum das_board_gpio_resource {
    DAS_BOARD_GPIO_ARDUINO_D3 = 0,
    DAS_BOARD_GPIO_ARDUINO_D4,
    DAS_BOARD_GPIO_COUNT
} das_board_gpio_resource_t;

das_gpio_pin_t das_board_gpio_pin(das_board_gpio_resource_t resource);

#endif
