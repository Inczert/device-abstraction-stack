// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_BOARD_H
#define DAS_BOARD_H

#include <stdbool.h>

#include <das/gpio.h>
#include <das/result.h>

/** Semantic user LEDs exposed by the selected DAS board/device layer. */
typedef enum das_board_led {
    DAS_BOARD_LED_GREEN = 0,
    DAS_BOARD_LED_YELLOW,
    DAS_BOARD_LED_RED,
    DAS_BOARD_LED_COUNT
} das_board_led_t;

/** Initialize one board LED as an output with the requested initial state. */
das_result_t das_board_led_init(das_board_led_t led, bool initially_on);

/** Initialize all board LEDs with the same initial state. */
das_result_t das_board_led_init_all(bool initially_on);

/** Set one board LED to its logical on/off state. */
das_result_t das_board_led_set(das_board_led_t led, bool on);

/** Toggle one board LED. */
das_result_t das_board_led_toggle(das_board_led_t led);

/** Return the logical output state currently latched for one board LED. */
bool das_board_led_is_on(das_board_led_t led);

/** Resolve a semantic board LED to its generic DAS GPIO pin. */
das_gpio_pin_t das_board_led_pin(das_board_led_t led);

#endif
