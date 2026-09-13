// SPDX-License-Identifier: Apache-2.0

#include <das/das.h>

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
        return 1;
    }

    static const uint8_t message[] = "DAS UART ready\r\n";
    if (das_uart_write(console, message, sizeof(message) - 1u) != DAS_OK) {
        return 2;
    }
    if (das_uart_flush(console) != DAS_OK) {
        return 3;
    }

    return 0;
}
