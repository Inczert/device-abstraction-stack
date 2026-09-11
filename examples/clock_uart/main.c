// SPDX-License-Identifier: Apache-2.0

#include <das/das.h>

#include <stdint.h>

volatile uint32_t g_das_example_core_hz;
volatile uint32_t g_das_example_uart_baud_hz;

int main(void) {
    uint32_t core_hz = 0u;
    uint32_t uart_baud_hz = 0u;

    if (das_clock_set_frequency(UINT32_C(400000000)) != DAS_OK) {
        return 1;
    }
    if (das_clock_get_core_frequency(&core_hz) != DAS_OK) {
        return 2;
    }
    g_das_example_core_hz = core_hz;

    const das_uart_config_t config = {
        .baud_rate = UINT32_C(115200),
        .data_bits = DAS_UART_DATA_BITS_8,
        .parity = DAS_UART_PARITY_NONE,
        .stop_bits = DAS_UART_STOP_BITS_1,
    };

    das_uart_t console = DAS_UART_INVALID;
    if (das_board_uart_init(DAS_BOARD_UART_STLINK_VCP, &config, &console) != DAS_OK) {
        return 3;
    }
    if (das_uart_get_baud_rate(console, &uart_baud_hz) != DAS_OK) {
        return 4;
    }
    g_das_example_uart_baud_hz = uart_baud_hz;

    static const uint8_t message[] = "DAS at 400 MHz, UART ready\r\n";
    if (das_uart_write(console, message, sizeof(message) - 1u) != DAS_OK) {
        return 5;
    }
    if (das_uart_flush(console) != DAS_OK) {
        return 6;
    }

    return 0;
}
