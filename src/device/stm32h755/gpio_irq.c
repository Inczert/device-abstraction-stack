// SPDX-License-Identifier: Apache-2.0

#include <das/gpio.h>

#include "stm32h755xx.h"

static bool gpio_pin_valid(das_gpio_pin_t pin) {
    return pin.port <= DAS_GPIO_PORT_K && pin.pin < 16u;
}

das_result_t das_gpio_interrupt_get_irq(das_gpio_pin_t pin, das_irq_t* irq) {
    IRQn_Type native_irq;

    if (!gpio_pin_valid(pin) || irq == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    switch (pin.pin) {
        case 0u: native_irq = EXTI0_IRQn; break;
        case 1u: native_irq = EXTI1_IRQn; break;
        case 2u: native_irq = EXTI2_IRQn; break;
        case 3u: native_irq = EXTI3_IRQn; break;
        case 4u: native_irq = EXTI4_IRQn; break;
        case 5u:
        case 6u:
        case 7u:
        case 8u:
        case 9u:
            native_irq = EXTI9_5_IRQn;
            break;
        default:
            native_irq = EXTI15_10_IRQn;
            break;
    }

    irq->storage = (uint32_t)(int32_t)native_irq;
    return DAS_OK;
}
