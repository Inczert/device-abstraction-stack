// SPDX-License-Identifier: Apache-2.0

#include <das/board_resources.h>
#include <das/clock.h>
#include <das/cortex_m/startup.h>
#include <das/time.h>
#include <das/uart.h>

#include "stm32h755xx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DAS_UART_TEST_MAGIC UINT32_C(0x44554152)
#define DAS_UART_TEST_HARDFAULT UINT32_C(0xe0080001)

#define DAS_UART_FLAG_TIMEOUT (UINT32_C(1) << 0u)
#define DAS_UART_FLAG_8N1     (UINT32_C(1) << 1u)
#define DAS_UART_FLAG_8E2     (UINT32_C(1) << 2u)
#define DAS_UART_FLAG_7O1     (UINT32_C(1) << 3u)
#define DAS_UART_FLAG_HANDLE  (UINT32_C(1) << 4u)
#define DAS_UART_REQUIRED_FLAGS UINT32_C(0x1f)

typedef struct das_uart_test_evidence {
    uint32_t magic;
    volatile uint32_t booted;
    volatile uint32_t error;
    volatile uint32_t heartbeat;
    volatile int32_t clock_result;
    volatile int32_t core_clock_result;
    volatile int32_t time_result;
    volatile int32_t timeout_result;
    volatile uint32_t core_hz;
    volatile uint32_t flags;
    volatile uint32_t baud_115200;
    volatile uint32_t baud_57600;
    volatile uint32_t baud_38400;
    volatile uint32_t bytes_checked;
    volatile uint32_t mismatch_expected;
    volatile uint32_t mismatch_actual;
} das_uart_test_evidence_t;

volatile das_uart_test_evidence_t g_das_uart_test_evidence = {
    .magic = DAS_UART_TEST_MAGIC,
};

static bool baud_close(uint32_t actual, uint32_t requested) {
    const uint32_t difference = actual > requested ? actual - requested : requested - actual;
    const uint32_t tolerance = requested / 50u + 1u; /* 2% */
    return difference <= tolerance;
}

static das_result_t run_loopback(const das_uart_config_t* config,
                                 const uint8_t* pattern,
                                 size_t pattern_size,
                                 uint32_t* actual_baud) {
    das_uart_t uart = DAS_UART_INVALID;
    das_result_t result = das_board_uart_init(DAS_BOARD_UART_ARDUINO, config, &uart);
    if (result != DAS_OK) {
        return result;
    }
    if (!das_uart_is_valid(uart)) {
        return DAS_ERROR_NOT_READY;
    }
    g_das_uart_test_evidence.flags |= DAS_UART_FLAG_HANDLE;

    result = das_uart_get_baud_rate(uart, actual_baud);
    if (result != DAS_OK || !baud_close(*actual_baud, config->baud_rate)) {
        return result == DAS_OK ? DAS_ERROR_IO : result;
    }

    for (size_t index = 0u; index < pattern_size; ++index) {
        uint8_t received = 0u;
        result = das_uart_write_timeout(uart, &pattern[index], 1u, 50u);
        if (result != DAS_OK) {
            return result;
        }
        result = das_uart_read_timeout(uart, &received, 1u, 50u);
        if (result != DAS_OK) {
            return result;
        }
        ++g_das_uart_test_evidence.bytes_checked;
        if (received != pattern[index]) {
            g_das_uart_test_evidence.mismatch_expected = pattern[index];
            g_das_uart_test_evidence.mismatch_actual = received;
            return DAS_ERROR_IO;
        }
    }

    return das_uart_flush(uart);
}

int main(void) {
#if defined(CORE_CM7)
    g_das_uart_test_evidence.clock_result = das_clock_set_frequency(UINT32_C(400000000));
#else
    g_das_uart_test_evidence.clock_result = DAS_OK;
#endif
    if (g_das_uart_test_evidence.clock_result != DAS_OK) {
        g_das_uart_test_evidence.error = UINT32_C(0x0801);
        for (;;) { __NOP(); }
    }

    uint32_t core_hz = 0u;
    g_das_uart_test_evidence.core_clock_result = das_clock_get_core_frequency(&core_hz);
    g_das_uart_test_evidence.core_hz = core_hz;
    if (g_das_uart_test_evidence.core_clock_result != DAS_OK) {
        g_das_uart_test_evidence.error = UINT32_C(0x0802);
        for (;;) { __NOP(); }
    }

    g_das_uart_test_evidence.time_result = das_time_init();
    if (g_das_uart_test_evidence.time_result != DAS_OK) {
        g_das_uart_test_evidence.error = UINT32_C(0x0803);
        for (;;) { __NOP(); }
    }

    const das_uart_config_t config_8n1 = {
        .baud_rate = UINT32_C(115200),
        .data_bits = DAS_UART_DATA_BITS_8,
        .parity = DAS_UART_PARITY_NONE,
        .stop_bits = DAS_UART_STOP_BITS_1,
    };

    das_uart_t timeout_uart = DAS_UART_INVALID;
    das_result_t result = das_board_uart_init(
        DAS_BOARD_UART_ARDUINO, &config_8n1, &timeout_uart);
    if (result != DAS_OK) {
        g_das_uart_test_evidence.error = UINT32_C(0x0804);
        for (;;) { __NOP(); }
    }

    uint8_t unused = 0u;
    g_das_uart_test_evidence.timeout_result =
        das_uart_read_timeout(timeout_uart, &unused, 1u, 5u);
    if (g_das_uart_test_evidence.timeout_result == DAS_ERROR_TIMEOUT) {
        g_das_uart_test_evidence.flags |= DAS_UART_FLAG_TIMEOUT;
    } else {
        g_das_uart_test_evidence.error = UINT32_C(0x0805);
        for (;;) { __NOP(); }
    }

    static const uint8_t pattern_8n1[] = {
        0x00u, 0xffu, 0x55u, 0xaau, 0x7eu, 0x81u, 0x11u, 0x22u, 0x44u, 0x88u,
        'D', 'A', 'S', '8', 'N', '1'
    };
    uint32_t actual_baud = 0u;
    result = run_loopback(&config_8n1,
                          pattern_8n1,
                          sizeof(pattern_8n1),
                          &actual_baud);
    g_das_uart_test_evidence.baud_115200 = actual_baud;
    if (result != DAS_OK) {
        g_das_uart_test_evidence.error = UINT32_C(0x0810) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    g_das_uart_test_evidence.flags |= DAS_UART_FLAG_8N1;

    const das_uart_config_t config_8e2 = {
        .baud_rate = UINT32_C(57600),
        .data_bits = DAS_UART_DATA_BITS_8,
        .parity = DAS_UART_PARITY_EVEN,
        .stop_bits = DAS_UART_STOP_BITS_2,
    };
    static const uint8_t pattern_8e2[] = {
        0x01u, 0x02u, 0x7fu, 0x80u, 0xfeu, 0x5au, 0xa5u, 'E', '2'
    };
    actual_baud = 0u;
    result = run_loopback(&config_8e2,
                          pattern_8e2,
                          sizeof(pattern_8e2),
                          &actual_baud);
    g_das_uart_test_evidence.baud_57600 = actual_baud;
    if (result != DAS_OK) {
        g_das_uart_test_evidence.error = UINT32_C(0x0820) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    g_das_uart_test_evidence.flags |= DAS_UART_FLAG_8E2;

    const das_uart_config_t config_7o1 = {
        .baud_rate = UINT32_C(38400),
        .data_bits = DAS_UART_DATA_BITS_7,
        .parity = DAS_UART_PARITY_ODD,
        .stop_bits = DAS_UART_STOP_BITS_1,
    };
    static const uint8_t pattern_7o1[] = {
        0x00u, 0x01u, 0x2au, 0x55u, 0x7eu, 0x7fu, '7', 'O', '1'
    };
    actual_baud = 0u;
    result = run_loopback(&config_7o1,
                          pattern_7o1,
                          sizeof(pattern_7o1),
                          &actual_baud);
    g_das_uart_test_evidence.baud_38400 = actual_baud;
    if (result != DAS_OK) {
        g_das_uart_test_evidence.error = UINT32_C(0x0830) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    g_das_uart_test_evidence.flags |= DAS_UART_FLAG_7O1;

    if (g_das_uart_test_evidence.flags != DAS_UART_REQUIRED_FLAGS) {
        g_das_uart_test_evidence.error = UINT32_C(0x0840);
        for (;;) { __NOP(); }
    }

    g_das_uart_test_evidence.booted = 1u;
    for (;;) {
        ++g_das_uart_test_evidence.heartbeat;
    }
}

void HardFault_Handler(void) {
    g_das_uart_test_evidence.error = DAS_UART_TEST_HARDFAULT;
    for (;;) { __NOP(); }
}
