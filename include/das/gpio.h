// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_GPIO_H
#define DAS_GPIO_H

#include <stdbool.h>
#include <stdint.h>

#include <das/result.h>

typedef enum das_gpio_port {
    DAS_GPIO_PORT_A = 0,
    DAS_GPIO_PORT_B,
    DAS_GPIO_PORT_C,
    DAS_GPIO_PORT_D,
    DAS_GPIO_PORT_E,
    DAS_GPIO_PORT_F,
    DAS_GPIO_PORT_G,
    DAS_GPIO_PORT_H,
    DAS_GPIO_PORT_I,
    DAS_GPIO_PORT_J,
    DAS_GPIO_PORT_K
} das_gpio_port_t;

typedef struct das_gpio_pin {
    das_gpio_port_t port;
    uint8_t pin;
} das_gpio_pin_t;

typedef enum das_gpio_mode {
    DAS_GPIO_MODE_INPUT = 0,
    DAS_GPIO_MODE_OUTPUT,
    DAS_GPIO_MODE_ALTERNATE,
    DAS_GPIO_MODE_ANALOG
} das_gpio_mode_t;

typedef enum das_gpio_pull {
    DAS_GPIO_PULL_NONE = 0,
    DAS_GPIO_PULL_UP,
    DAS_GPIO_PULL_DOWN
} das_gpio_pull_t;

typedef enum das_gpio_output_type {
    DAS_GPIO_OUTPUT_PUSH_PULL = 0,
    DAS_GPIO_OUTPUT_OPEN_DRAIN
} das_gpio_output_type_t;

typedef enum das_gpio_speed {
    DAS_GPIO_SPEED_LOW = 0,
    DAS_GPIO_SPEED_MEDIUM,
    DAS_GPIO_SPEED_HIGH,
    DAS_GPIO_SPEED_VERY_HIGH
} das_gpio_speed_t;

typedef struct das_gpio_config {
    das_gpio_mode_t mode;
    das_gpio_pull_t pull;
    das_gpio_output_type_t output_type;
    das_gpio_speed_t speed;
    uint8_t alternate;
    bool initial_high;
} das_gpio_config_t;

typedef enum das_gpio_interrupt_edge {
    DAS_GPIO_INTERRUPT_RISING = 1,
    DAS_GPIO_INTERRUPT_FALLING = 2,
    DAS_GPIO_INTERRUPT_BOTH = 3
} das_gpio_interrupt_edge_t;

das_result_t das_gpio_configure(das_gpio_pin_t pin, const das_gpio_config_t* config);
das_result_t das_gpio_input_init(das_gpio_pin_t pin, das_gpio_pull_t pull);
das_result_t das_gpio_output_init(das_gpio_pin_t pin, bool initial_high);
das_result_t das_gpio_output_init_ex(das_gpio_pin_t pin,
                                     das_gpio_output_type_t output_type,
                                     das_gpio_pull_t pull,
                                     das_gpio_speed_t speed,
                                     bool initial_high);
das_result_t das_gpio_write(das_gpio_pin_t pin, bool high);
das_result_t das_gpio_toggle(das_gpio_pin_t pin);
bool das_gpio_read_output(das_gpio_pin_t pin);
bool das_gpio_read_input(das_gpio_pin_t pin);

das_result_t das_gpio_interrupt_configure(das_gpio_pin_t pin,
                                           das_gpio_interrupt_edge_t edge);
das_result_t das_gpio_interrupt_enable(das_gpio_pin_t pin, bool enabled);
bool das_gpio_interrupt_pending(das_gpio_pin_t pin);
das_result_t das_gpio_interrupt_clear(das_gpio_pin_t pin);

#endif
