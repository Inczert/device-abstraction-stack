// SPDX-License-Identifier: Apache-2.0

#include <das/gpio.h>

#include "stm32h755xx.h"

static GPIO_TypeDef* gpio_port(das_gpio_port_t port) {
    switch (port) {
        case DAS_GPIO_PORT_A: return GPIOA;
        case DAS_GPIO_PORT_B: return GPIOB;
        case DAS_GPIO_PORT_C: return GPIOC;
        case DAS_GPIO_PORT_D: return GPIOD;
        case DAS_GPIO_PORT_E: return GPIOE;
        case DAS_GPIO_PORT_F: return GPIOF;
        case DAS_GPIO_PORT_G: return GPIOG;
        case DAS_GPIO_PORT_H: return GPIOH;
        case DAS_GPIO_PORT_I: return GPIOI;
        case DAS_GPIO_PORT_J: return GPIOJ;
        case DAS_GPIO_PORT_K: return GPIOK;
        default: return 0;
    }
}

static uint32_t gpio_clock_mask(das_gpio_port_t port) {
    switch (port) {
        case DAS_GPIO_PORT_A: return RCC_AHB4ENR_GPIOAEN;
        case DAS_GPIO_PORT_B: return RCC_AHB4ENR_GPIOBEN;
        case DAS_GPIO_PORT_C: return RCC_AHB4ENR_GPIOCEN;
        case DAS_GPIO_PORT_D: return RCC_AHB4ENR_GPIODEN;
        case DAS_GPIO_PORT_E: return RCC_AHB4ENR_GPIOEEN;
        case DAS_GPIO_PORT_F: return RCC_AHB4ENR_GPIOFEN;
        case DAS_GPIO_PORT_G: return RCC_AHB4ENR_GPIOGEN;
        case DAS_GPIO_PORT_H: return RCC_AHB4ENR_GPIOHEN;
        case DAS_GPIO_PORT_I: return RCC_AHB4ENR_GPIOIEN;
        case DAS_GPIO_PORT_J: return RCC_AHB4ENR_GPIOJEN;
        case DAS_GPIO_PORT_K: return RCC_AHB4ENR_GPIOKEN;
        default: return 0u;
    }
}

static uint32_t gpio_port_index(das_gpio_port_t port) {
    switch (port) {
        case DAS_GPIO_PORT_A: return 0u;
        case DAS_GPIO_PORT_B: return 1u;
        case DAS_GPIO_PORT_C: return 2u;
        case DAS_GPIO_PORT_D: return 3u;
        case DAS_GPIO_PORT_E: return 4u;
        case DAS_GPIO_PORT_F: return 5u;
        case DAS_GPIO_PORT_G: return 6u;
        case DAS_GPIO_PORT_H: return 7u;
        case DAS_GPIO_PORT_I: return 8u;
        case DAS_GPIO_PORT_J: return 9u;
        case DAS_GPIO_PORT_K: return 10u;
        default: return UINT32_MAX;
    }
}

static das_result_t resolve_pin(das_gpio_pin_t pin, GPIO_TypeDef** out_port) {
    GPIO_TypeDef* port = gpio_port(pin.port);
    if (out_port == 0 || port == 0 || pin.pin >= 16u) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    *out_port = port;
    return DAS_OK;
}

static bool mode_valid(das_gpio_mode_t mode) {
    switch (mode) {
        case DAS_GPIO_MODE_INPUT:
        case DAS_GPIO_MODE_OUTPUT:
        case DAS_GPIO_MODE_ALTERNATE:
        case DAS_GPIO_MODE_ANALOG:
            return true;
        default:
            return false;
    }
}

static bool pull_valid(das_gpio_pull_t pull) {
    switch (pull) {
        case DAS_GPIO_PULL_NONE:
        case DAS_GPIO_PULL_UP:
        case DAS_GPIO_PULL_DOWN:
            return true;
        default:
            return false;
    }
}

static bool output_type_valid(das_gpio_output_type_t output_type) {
    return output_type == DAS_GPIO_OUTPUT_PUSH_PULL ||
           output_type == DAS_GPIO_OUTPUT_OPEN_DRAIN;
}

static bool speed_valid(das_gpio_speed_t speed) {
    switch (speed) {
        case DAS_GPIO_SPEED_LOW:
        case DAS_GPIO_SPEED_MEDIUM:
        case DAS_GPIO_SPEED_HIGH:
        case DAS_GPIO_SPEED_VERY_HIGH:
            return true;
        default:
            return false;
    }
}

static bool config_valid(const das_gpio_config_t* config) {
    if (config == 0 ||
        !mode_valid(config->mode) ||
        !pull_valid(config->pull) ||
        !output_type_valid(config->output_type) ||
        !speed_valid(config->speed)) {
        return false;
    }
    return config->mode != DAS_GPIO_MODE_ALTERNATE || config->alternate < 16u;
}

das_result_t das_gpio_configure(das_gpio_pin_t pin, const das_gpio_config_t* config) {
    GPIO_TypeDef* port;
    const uint32_t clock_mask = gpio_clock_mask(pin.port);
    if (resolve_pin(pin, &port) != DAS_OK || clock_mask == 0u || !config_valid(config)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    RCC->AHB4ENR |= clock_mask;
    (void)RCC->AHB4ENR;
    __DSB();

    const uint32_t bit = UINT32_C(1) << pin.pin;
    const uint32_t shift = (uint32_t)pin.pin * 2u;
    const uint32_t field_mask = UINT32_C(3) << shift;

    if (config->mode == DAS_GPIO_MODE_OUTPUT || config->mode == DAS_GPIO_MODE_ALTERNATE) {
        port->BSRR = config->initial_high ? bit : (bit << 16u);
    }

    if (config->output_type == DAS_GPIO_OUTPUT_OPEN_DRAIN) {
        port->OTYPER |= bit;
    } else {
        port->OTYPER &= ~bit;
    }

    port->OSPEEDR = (port->OSPEEDR & ~field_mask) |
                    ((uint32_t)config->speed << shift);
    port->PUPDR = (port->PUPDR & ~field_mask) |
                  ((uint32_t)config->pull << shift);

    if (config->mode == DAS_GPIO_MODE_ALTERNATE) {
        const uint32_t afr_index = (uint32_t)pin.pin / 8u;
        const uint32_t afr_shift = ((uint32_t)pin.pin % 8u) * 4u;
        const uint32_t afr_mask = UINT32_C(0xf) << afr_shift;
        port->AFR[afr_index] = (port->AFR[afr_index] & ~afr_mask) |
                               ((uint32_t)config->alternate << afr_shift);
    }

    port->MODER = (port->MODER & ~field_mask) |
                  ((uint32_t)config->mode << shift);
    __DSB();
    return DAS_OK;
}

das_result_t das_gpio_input_init(das_gpio_pin_t pin, das_gpio_pull_t pull) {
    const das_gpio_config_t config = {
        .mode = DAS_GPIO_MODE_INPUT,
        .pull = pull,
        .output_type = DAS_GPIO_OUTPUT_PUSH_PULL,
        .speed = DAS_GPIO_SPEED_LOW,
        .alternate = 0u,
        .initial_high = false,
    };
    return das_gpio_configure(pin, &config);
}

das_result_t das_gpio_output_init(das_gpio_pin_t pin, bool initial_high) {
    return das_gpio_output_init_ex(pin,
                                   DAS_GPIO_OUTPUT_PUSH_PULL,
                                   DAS_GPIO_PULL_NONE,
                                   DAS_GPIO_SPEED_LOW,
                                   initial_high);
}

das_result_t das_gpio_output_init_ex(das_gpio_pin_t pin,
                                     das_gpio_output_type_t output_type,
                                     das_gpio_pull_t pull,
                                     das_gpio_speed_t speed,
                                     bool initial_high) {
    const das_gpio_config_t config = {
        .mode = DAS_GPIO_MODE_OUTPUT,
        .pull = pull,
        .output_type = output_type,
        .speed = speed,
        .alternate = 0u,
        .initial_high = initial_high,
    };
    return das_gpio_configure(pin, &config);
}

das_result_t das_gpio_write(das_gpio_pin_t pin, bool high) {
    GPIO_TypeDef* port;
    if (resolve_pin(pin, &port) != DAS_OK) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    const uint32_t bit = UINT32_C(1) << pin.pin;
    port->BSRR = high ? bit : (bit << 16u);
    return DAS_OK;
}

das_result_t das_gpio_toggle(das_gpio_pin_t pin) {
    GPIO_TypeDef* port;
    if (resolve_pin(pin, &port) != DAS_OK) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    const uint32_t bit = UINT32_C(1) << pin.pin;
    port->BSRR = (port->ODR & bit) != 0u ? (bit << 16u) : bit;
    return DAS_OK;
}

bool das_gpio_read_output(das_gpio_pin_t pin) {
    GPIO_TypeDef* port;
    if (resolve_pin(pin, &port) != DAS_OK) {
        return false;
    }
    return (port->ODR & (UINT32_C(1) << pin.pin)) != 0u;
}

bool das_gpio_read_input(das_gpio_pin_t pin) {
    GPIO_TypeDef* port;
    if (resolve_pin(pin, &port) != DAS_OK) {
        return false;
    }
    return (port->IDR & (UINT32_C(1) << pin.pin)) != 0u;
}

das_result_t das_gpio_interrupt_configure(das_gpio_pin_t pin,
                                           das_gpio_interrupt_edge_t edge) {
    GPIO_TypeDef* unused_port;
    const uint32_t port_index = gpio_port_index(pin.port);
    const bool edge_valid = edge == DAS_GPIO_INTERRUPT_RISING ||
                            edge == DAS_GPIO_INTERRUPT_FALLING ||
                            edge == DAS_GPIO_INTERRUPT_BOTH;
    if (resolve_pin(pin, &unused_port) != DAS_OK ||
        port_index == UINT32_MAX ||
        !edge_valid) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    RCC_C1->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC_C1->APB4ENR;
    __DSB();

    const uint32_t exti_index = (uint32_t)pin.pin / 4u;
    const uint32_t exti_shift = ((uint32_t)pin.pin % 4u) * 4u;
    const uint32_t exti_mask = UINT32_C(0xf) << exti_shift;
    const uint32_t line = UINT32_C(1) << pin.pin;

    SYSCFG->EXTICR[exti_index] =
        (SYSCFG->EXTICR[exti_index] & ~exti_mask) | (port_index << exti_shift);

    if ((edge & DAS_GPIO_INTERRUPT_RISING) != 0) {
        EXTI->RTSR1 |= line;
    } else {
        EXTI->RTSR1 &= ~line;
    }
    if ((edge & DAS_GPIO_INTERRUPT_FALLING) != 0) {
        EXTI->FTSR1 |= line;
    } else {
        EXTI->FTSR1 &= ~line;
    }

    EXTI->IMR1 &= ~line;
    EXTI->PR1 = line;
    __DSB();
    return DAS_OK;
}

das_result_t das_gpio_interrupt_enable(das_gpio_pin_t pin, bool enabled) {
    GPIO_TypeDef* unused_port;
    if (resolve_pin(pin, &unused_port) != DAS_OK) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    const uint32_t line = UINT32_C(1) << pin.pin;
    if (enabled) {
        EXTI->IMR1 |= line;
    } else {
        EXTI->IMR1 &= ~line;
    }
    __DSB();
    return DAS_OK;
}

bool das_gpio_interrupt_pending(das_gpio_pin_t pin) {
    GPIO_TypeDef* unused_port;
    if (resolve_pin(pin, &unused_port) != DAS_OK) {
        return false;
    }
    return (EXTI->PR1 & (UINT32_C(1) << pin.pin)) != 0u;
}

das_result_t das_gpio_interrupt_clear(das_gpio_pin_t pin) {
    GPIO_TypeDef* unused_port;
    if (resolve_pin(pin, &unused_port) != DAS_OK) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    EXTI->PR1 = UINT32_C(1) << pin.pin;
    __DSB();
    return DAS_OK;
}
