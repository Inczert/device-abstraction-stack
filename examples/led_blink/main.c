// SPDX-License-Identifier: Apache-2.0

#include <das/board.h>
#include <das/time.h>

static void halt(void) {
    for (;;) {
    }
}

int main(void) {
    if (das_board_led_init(DAS_BOARD_LED_GREEN, false) != DAS_OK) {
        halt();
    }
    if (das_time_init() != DAS_OK) {
        halt();
    }

    for (;;) {
        if (das_board_led_toggle(DAS_BOARD_LED_GREEN) != DAS_OK) {
            halt();
        }
        if (das_delay_ms(500u) != DAS_OK) {
            halt();
        }
    }
}
