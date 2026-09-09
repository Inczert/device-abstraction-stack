// SPDX-License-Identifier: Apache-2.0

#include <das/board_resources.h>
#include <das/uart.h>

#include <stddef.h>
#include <stdint.h>

int main(void) {
    const das_uart_config_t config = {
        .baud_rate = UINT32_C(115200),
        .data_bits = DAS_UART_DATA_BITS_8,
        .parity = DAS_UART_PARITY_NONE,
        .stop_bits = DAS_UART_STOP_BITS_1,
    };

    das_uart_t console = DAS_UART_INVALID;
    if (das_board_uart_init(DAS_BOARD_UART_STLINK_VCP, &config, &console) != DAS_OK) {
        for (;;) {
        }
    }

    static const uint8_t message[] = "DAS UART ready\r\n";
    (void)das_uart_write(console, message, sizeof(message) - 1u);

    for (;;) {
    }
}
