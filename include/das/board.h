// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_BOARD_H
#define DAS_BOARD_H

#include <stdbool.h>

#include <das/board_resources.h>
#include <das/gpio.h>
#include <das/irq.h>
#include <das/result.h>

/** Semantic user LEDs exposed by the selected DAS board/device layer. */
typedef enum das_board_led {
    DAS_BOARD_LED_GREEN = 0,
    DAS_BOARD_LED_YELLOW,
    DAS_BOARD_LED_RED,
    DAS_BOARD_LED_COUNT
} das_board_led_t;

/** Semantic push-buttons exposed by the selected DAS board layer. */
typedef enum das_board_button {
    DAS_BOARD_BUTTON_USER = 0,
    DAS_BOARD_BUTTON_COUNT
} das_board_button_t;

/** Logical button events. The board layer owns electrical polarity. */
typedef enum das_board_button_event {
    DAS_BOARD_BUTTON_EVENT_PRESS = 0,
    DAS_BOARD_BUTTON_EVENT_RELEASE,
    DAS_BOARD_BUTTON_EVENT_BOTH
} das_board_button_event_t;

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

/** Initialize a semantic board button as an input using the board's bias policy. */
das_result_t das_board_button_init(das_board_button_t button);

/** Return the logical pressed state; electrical polarity remains a board detail. */
bool das_board_button_is_pressed(das_board_button_t button);

/** Resolve a semantic board button to its generic DAS GPIO pin. */
das_gpio_pin_t das_board_button_pin(das_board_button_t button);

/** Configure press/release edge detection for a semantic board button. */
das_result_t das_board_button_interrupt_configure(
    das_board_button_t button,
    das_board_button_event_t event);

/** Mask/unmask the button's interrupt source. Controller state is separate. */
das_result_t das_board_button_interrupt_enable(das_board_button_t button,
                                               bool enabled);

/** Return true when the button source has a pending interrupt event. */
bool das_board_button_interrupt_pending(das_board_button_t button);

/** Clear the button source's pending interrupt event. */
das_result_t das_board_button_interrupt_clear(das_board_button_t button);

/** Resolve the button source to the generic DAS interrupt-controller handle. */
das_result_t das_board_button_interrupt_get_irq(das_board_button_t button,
                                                das_irq_t* irq);

#endif
