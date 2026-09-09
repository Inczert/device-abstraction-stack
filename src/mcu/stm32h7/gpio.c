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

static das_result_t resolve_pin(das_gpio_pin_t pin, GPIO_TypeDef** out_port) {
    GPIO_TypeDef* port = gpio_port(pin.port);
    if (out_port == 0 || port == 0 || pin.pin >= 16u) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    *out_port = port;
    return DAS_OK;
}

das_result_t das_gpio_output_init(das_gpio_pin_t pin, bool initial_high) {
    GPIO_TypeDef* port;
    const uint32_t clock_mask = gpio_clock_mask(pin.port);
    if (resolve_pin(pin, &port) != DAS_OK || clock_mask == 0u) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    RCC->AHB4ENR |= clock_mask;
    (void)RCC->AHB4ENR;
    __DSB();

    const uint32_t bit = UINT32_C(1) << pin.pin;
    const uint32_t shift = (uint32_t)pin.pin * 2u;
    const uint32_t field_mask = UINT32_C(3) << shift;

    port->BSRR = initial_high ? bit : (bit << 16u);
    port->OTYPER &= ~bit;
    port->OSPEEDR &= ~field_mask;
    port->PUPDR &= ~field_mask;
    port->MODER = (port->MODER & ~field_mask) | (UINT32_C(1) << shift);
    __DSB();
    return DAS_OK;
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
