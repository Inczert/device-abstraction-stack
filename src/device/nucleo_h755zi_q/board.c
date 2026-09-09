// SPDX-License-Identifier: Apache-2.0

#include <das/board.h>

static const das_gpio_pin_t LED_PINS[DAS_BOARD_LED_COUNT] = {
    [DAS_BOARD_LED_GREEN] = {DAS_GPIO_PORT_B, 0u},
    [DAS_BOARD_LED_YELLOW] = {DAS_GPIO_PORT_E, 1u},
    [DAS_BOARD_LED_RED] = {DAS_GPIO_PORT_B, 14u},
};

static bool led_valid(das_board_led_t led) {
    return led < DAS_BOARD_LED_COUNT;
}

das_gpio_pin_t das_board_led_pin(das_board_led_t led) {
    if (!led_valid(led)) {
        return (das_gpio_pin_t){DAS_GPIO_PORT_A, UINT8_C(0xff)};
    }
    return LED_PINS[led];
}

das_result_t das_board_led_init(das_board_led_t led, bool initially_on) {
    if (!led_valid(led)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    return das_gpio_output_init(LED_PINS[led], initially_on);
}

das_result_t das_board_led_init_all(bool initially_on) {
    for (int led = DAS_BOARD_LED_GREEN; led < DAS_BOARD_LED_COUNT; ++led) {
        das_result_t result = das_board_led_init((das_board_led_t)led, initially_on);
        if (result != DAS_OK) {
            return result;
        }
    }
    return DAS_OK;
}

das_result_t das_board_led_set(das_board_led_t led, bool on) {
    if (!led_valid(led)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    return das_gpio_write(LED_PINS[led], on);
}

das_result_t das_board_led_toggle(das_board_led_t led) {
    if (!led_valid(led)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    return das_gpio_toggle(LED_PINS[led]);
}

bool das_board_led_is_on(das_board_led_t led) {
    return led_valid(led) && das_gpio_read_output(LED_PINS[led]);
}
