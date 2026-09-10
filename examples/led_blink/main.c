// SPDX-License-Identifier: Apache-2.0

#include <das/das.h>

int main(void) {
    if (das_board_led_init(DAS_BOARD_LED_GREEN, false) != DAS_OK) {
        return 1;
    }
    if (das_time_init() != DAS_OK) {
        return 2;
    }

    for (;;) {
        if (das_board_led_toggle(DAS_BOARD_LED_GREEN) != DAS_OK) {
            return 3;
        }
        if (das_delay_ms(500u) != DAS_OK) {
            return 4;
        }
    }
}
