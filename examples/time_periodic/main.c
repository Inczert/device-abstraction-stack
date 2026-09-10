// SPDX-License-Identifier: Apache-2.0

#include <das/board.h>
#include <das/clock.h>
#include <das/time.h>

int main(void) {
    if (das_clock_set_frequency(200000000u) != DAS_OK) {
        return 1;
    }
    if (das_board_led_init(DAS_BOARD_LED_GREEN, false) != DAS_OK) {
        return 2;
    }
    if (das_time_init() != DAS_OK) {
        return 3;
    }

    das_time_ms_t last_toggle = das_time_now_ms();
    for (;;) {
        if (das_time_interval_elapsed(last_toggle, 500u)) {
            last_toggle += 500u;
            if (das_board_led_toggle(DAS_BOARD_LED_GREEN) != DAS_OK) {
                return 4;
            }
        }
    }
}
