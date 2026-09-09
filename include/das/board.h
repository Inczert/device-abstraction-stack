// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_BOARD_H
#define DAS_BOARD_H

#include <stdbool.h>

#include <das/gpio.h>
#include <das/result.h>

typedef enum das_board_led {
    DAS_BOARD_LED_GREEN = 0,
    DAS_BOARD_LED_YELLOW,
    DAS_BOARD_LED_RED,
    DAS_BOARD_LED_COUNT
} das_board_led_t;

das_result_t das_board_led_init(das_board_led_t led, bool initially_on);
das_result_t das_board_led_init_all(bool initially_on);
das_result_t das_board_led_set(das_board_led_t led, bool on);
das_result_t das_board_led_toggle(das_board_led_t led);
bool das_board_led_is_on(das_board_led_t led);
das_gpio_pin_t das_board_led_pin(das_board_led_t led);

#endif
