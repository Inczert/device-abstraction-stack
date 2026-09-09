// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_GPIO_H
#define DAS_GPIO_H

#include <stdbool.h>
#include <stdint.h>

#include <das/irq.h>
#include <das/result.h>

/** Generic GPIO port identifier. Backends may reject ports absent on the target. */
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

/** Generic GPIO pin expressed as port plus pin number. */
typedef struct das_gpio_pin {
    das_gpio_port_t port;
    uint8_t pin;
} das_gpio_pin_t;

/** GPIO operating mode. */
typedef enum das_gpio_mode {
    DAS_GPIO_MODE_INPUT = 0,
    DAS_GPIO_MODE_OUTPUT,
    DAS_GPIO_MODE_ALTERNATE,
    DAS_GPIO_MODE_ANALOG
} das_gpio_mode_t;

/** Internal pull configuration. */
typedef enum das_gpio_pull {
    DAS_GPIO_PULL_NONE = 0,
    DAS_GPIO_PULL_UP,
    DAS_GPIO_PULL_DOWN
} das_gpio_pull_t;

/** GPIO output driver type. */
typedef enum das_gpio_output_type {
    DAS_GPIO_OUTPUT_PUSH_PULL = 0,
    DAS_GPIO_OUTPUT_OPEN_DRAIN
} das_gpio_output_type_t;

/** GPIO electrical speed/slew setting. */
typedef enum das_gpio_speed {
    DAS_GPIO_SPEED_LOW = 0,
    DAS_GPIO_SPEED_MEDIUM,
    DAS_GPIO_SPEED_HIGH,
    DAS_GPIO_SPEED_VERY_HIGH
} das_gpio_speed_t;

/** Complete GPIO configuration used by das_gpio_configure(). */
typedef struct das_gpio_config {
    das_gpio_mode_t mode;
    das_gpio_pull_t pull;
    das_gpio_output_type_t output_type;
    das_gpio_speed_t speed;
    /** Backend alternate-function selector; STM32H7 accepts 0..15. */
    uint8_t alternate;
    /** Initial output latch for output/alternate modes. */
    bool initial_high;
} das_gpio_config_t;

/** GPIO interrupt edge selection. */
typedef enum das_gpio_interrupt_edge {
    DAS_GPIO_INTERRUPT_RISING = 1,
    DAS_GPIO_INTERRUPT_FALLING = 2,
    DAS_GPIO_INTERRUPT_BOTH = 3
} das_gpio_interrupt_edge_t;

/** Configure all generic properties of a GPIO pin. */
das_result_t das_gpio_configure(das_gpio_pin_t pin, const das_gpio_config_t* config);

/** Configure a GPIO as an input with the requested internal pull. */
das_result_t das_gpio_input_init(das_gpio_pin_t pin, das_gpio_pull_t pull);

/** Configure a low-speed push-pull GPIO output with no pull. */
das_result_t das_gpio_output_init(das_gpio_pin_t pin, bool initial_high);

/** Configure a GPIO output with explicit driver, pull, speed, and initial state. */
das_result_t das_gpio_output_init_ex(das_gpio_pin_t pin,
                                     das_gpio_output_type_t output_type,
                                     das_gpio_pull_t pull,
                                     das_gpio_speed_t speed,
                                     bool initial_high);

/** Write the logical output latch. For open-drain, high means released. */
das_result_t das_gpio_write(das_gpio_pin_t pin, bool high);

/** Toggle the logical output latch. */
das_result_t das_gpio_toggle(das_gpio_pin_t pin);

/** Read the output latch state, not necessarily the physical pad level. */
bool das_gpio_read_output(das_gpio_pin_t pin);

/** Read the physical GPIO input level. */
bool das_gpio_read_input(das_gpio_pin_t pin);

/**
 * Route/configure a GPIO interrupt source for the requested edge(s).
 *
 * This configures the backend GPIO/EXTI source path. Interrupt-controller
 * priority/enabling is controlled separately through the DAS IRQ API.
 */
das_result_t das_gpio_interrupt_configure(das_gpio_pin_t pin,
                                           das_gpio_interrupt_edge_t edge);

/** Enable or disable the backend GPIO interrupt source/event line. */
das_result_t das_gpio_interrupt_enable(das_gpio_pin_t pin, bool enabled);

/**
 * Resolve the interrupt-controller line used by this GPIO interrupt source.
 *
 * Several GPIO pins may share one controller IRQ. The returned handle is
 * therefore a controller-line handle, not a unique GPIO-event identifier.
 */
das_result_t das_gpio_interrupt_get_irq(das_gpio_pin_t pin, das_irq_t* irq);

/** Return whether interrupt-source pending state is set for this GPIO line. */
bool das_gpio_interrupt_pending(das_gpio_pin_t pin);

/** Clear interrupt-source pending state for this GPIO line. */
das_result_t das_gpio_interrupt_clear(das_gpio_pin_t pin);

#endif
