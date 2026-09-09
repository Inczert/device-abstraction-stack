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

das_result_t das_gpio_output_init(das_gpio_pin_t pin, bool initial_high);
das_result_t das_gpio_write(das_gpio_pin_t pin, bool high);
das_result_t das_gpio_toggle(das_gpio_pin_t pin);
bool das_gpio_read_output(das_gpio_pin_t pin);
bool das_gpio_read_input(das_gpio_pin_t pin);

#endif
