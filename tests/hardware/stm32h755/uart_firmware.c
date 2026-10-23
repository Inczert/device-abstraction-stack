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
#define DAS_UART_FLAG_TRY_IO  (UINT32_C(1) << 5u)
#define DAS_UART_FLAG_IRQ_RX (UINT32_C(1) << 6u)
#define DAS_UART_REQUIRED_FLAGS UINT32_C(0x7f)

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


    /*
     * Nonblocking primitives use the same physical D1->D0 loopback. They
     * report partial progress and never wait for a hardware flag themselves.
     * This checks register semantics, not interrupt buffering under load.
     */
    result = das_board_uart_init(DAS_BOARD_UART_ARDUINO, &config_8n1, &timeout_uart);
    if (result != DAS_OK) {
        g_das_uart_test_evidence.error = UINT32_C(0x0850);
        for (;;) { __NOP(); }
    }
    size_t transferred = 99u;
    uint8_t try_byte = 0u;
    result = das_uart_try_read(timeout_uart, &try_byte, 1u, &transferred);
    if (result != DAS_OK || transferred != 0u) {
        g_das_uart_test_evidence.error = UINT32_C(0x0851);
        for (;;) { __NOP(); }
    }
    static const uint8_t try_pattern[] = {0x00u, 0x5au, 0xa5u, 0xffu};
    for (size_t index = 0u; index < sizeof(try_pattern); ++index) {
        bool sent = false;
        for (uint32_t attempt = 0u; attempt < UINT32_C(1000000); ++attempt) {
            transferred = 99u;
            result = das_uart_try_write(timeout_uart, &try_pattern[index], 1u, &transferred);
            if (result != DAS_OK || transferred > 1u) {
                g_das_uart_test_evidence.error = UINT32_C(0x0852);
                for (;;) { __NOP(); }
            }
            if (transferred == 1u) {
                sent = true;
                break;
            }
        }
        if (!sent) {
            g_das_uart_test_evidence.error = UINT32_C(0x0853);
            for (;;) { __NOP(); }
        }
        bool got = false;
        for (uint32_t attempt = 0u; attempt < UINT32_C(1000000); ++attempt) {
            transferred = 99u;
            result = das_uart_try_read(timeout_uart, &try_byte, 1u, &transferred);
            if (result != DAS_OK || transferred > 1u) {
                g_das_uart_test_evidence.error = UINT32_C(0x0854);
                for (;;) { __NOP(); }
            }
            if (transferred == 1u) {
                got = true;
                break;
            }
        }
        if (!got || try_byte != try_pattern[index]) {
            g_das_uart_test_evidence.error = UINT32_C(0x0855);
            g_das_uart_test_evidence.mismatch_expected = try_pattern[index];
            g_das_uart_test_evidence.mismatch_actual = try_byte;
            for (;;) { __NOP(); }
        }
        ++g_das_uart_test_evidence.bytes_checked;
    }
    g_das_uart_test_evidence.flags |= DAS_UART_FLAG_TRY_IO;


    /*
     * Interrupt RX path: ring storage is caller owned, IRQ is provided by
     * the optional separate module, and the data path does not poll RDR.
     */
    uint8_t rx_storage[8] = {0u};
    result = das_uart_rx_buffer_start(timeout_uart, rx_storage, sizeof(rx_storage));
    if (result != DAS_OK) {
        g_das_uart_test_evidence.error = UINT32_C(0x0860);
        for (;;) { __NOP(); }
    }
    static const uint8_t irq_pattern[] = {0x19u, 0xe1u, 0x00u, 0x7fu};
    for (size_t index = 0u; index < sizeof(irq_pattern); ++index) {
        result = das_uart_write_timeout(timeout_uart, &irq_pattern[index], 1u, 100u);
        if (result != DAS_OK) {
            g_das_uart_test_evidence.error = UINT32_C(0x0861);
            for (;;) { __NOP(); }
        }
        size_t got = 0u;
        uint8_t byte = 0u;
        bool received_byte = false;
        for (uint32_t attempt = 0u; attempt < UINT32_C(1000000); ++attempt) {
            result = das_uart_rx_buffer_read(timeout_uart, &byte, 1u, &got);
            if (result != DAS_OK || got > 1u) {
                g_das_uart_test_evidence.error = UINT32_C(0x0862);
                for (;;) { __NOP(); }
            }
            if (got == 1u) {
                received_byte = true;
                break;
            }
        }
        if (!received_byte || byte != irq_pattern[index]) {
            g_das_uart_test_evidence.error = UINT32_C(0x0863);
            g_das_uart_test_evidence.mismatch_expected = irq_pattern[index];
            g_das_uart_test_evidence.mismatch_actual = byte;
            for (;;) { __NOP(); }
        }
        ++g_das_uart_test_evidence.bytes_checked;
    }
    das_uart_rx_stats_t stats = {0};
    result = das_uart_rx_buffer_stats(timeout_uart, &stats);
    if (result != DAS_OK || stats.bytes_stored != sizeof(irq_pattern) ||
        stats.bytes_dropped != 0u || stats.hardware_errors != 0u ||
        stats.bytes_available != 0u || stats.capacity != sizeof(rx_storage)) {
        g_das_uart_test_evidence.error = UINT32_C(0x0864);
        for (;;) { __NOP(); }
    }
    /*
     * Keep the IRQ ring full while sending 12 more bytes into an 8-byte
     * buffer. The first eight must survive; four newer bytes are dropped.
     */
    static const uint8_t overflow_pattern[] = {
        0x10u, 0x21u, 0x32u, 0x43u, 0x54u, 0x65u,
        0x76u, 0x87u, 0x98u, 0xa9u, 0xbau, 0xcbu
    };
    for (size_t index = 0u; index < sizeof(overflow_pattern); ++index) {
        result = das_uart_write_timeout(timeout_uart, &overflow_pattern[index], 1u, 100u);
        if (result != DAS_OK) {
            g_das_uart_test_evidence.error = UINT32_C(0x0866);
            for (;;) { __NOP(); }
        }
    }
    result = das_uart_flush(timeout_uart);
    if (result != DAS_OK) {
        g_das_uart_test_evidence.error = UINT32_C(0x0867);
        for (;;) { __NOP(); }
    }
    /*
     * The final RXNE interrupt can be pending when the TX shift register
     * first asserts TC. Observe completion with a bounded poll of counters
     * rather than interpreting interrupt scheduling latency as data loss.
     */
    bool overflow_accounted = false;
    for (uint32_t attempt = 0u; attempt < UINT32_C(1000000); ++attempt) {
        stats = (das_uart_rx_stats_t){0};
        result = das_uart_rx_buffer_stats(timeout_uart, &stats);
        if (result != DAS_OK || stats.hardware_errors != 0u) {
            break;
        }
        if (stats.bytes_stored == sizeof(irq_pattern) + sizeof(rx_storage) &&
            stats.bytes_dropped == sizeof(overflow_pattern) - sizeof(rx_storage) &&
            stats.bytes_available == sizeof(rx_storage)) {
            overflow_accounted = true;
            break;
        }
    }
    if (!overflow_accounted) {
        g_das_uart_test_evidence.error = UINT32_C(0x0868);
        for (;;) { __NOP(); }
    }
    for (size_t index = 0u; index < sizeof(rx_storage); ++index) {
        size_t got = 0u;
        uint8_t byte = 0u;
        result = das_uart_rx_buffer_read(timeout_uart, &byte, 1u, &got);
        if (result != DAS_OK || got != 1u || byte != overflow_pattern[index]) {
            g_das_uart_test_evidence.error = UINT32_C(0x0869);
            g_das_uart_test_evidence.mismatch_expected = overflow_pattern[index];
            g_das_uart_test_evidence.mismatch_actual = byte;
            for (;;) { __NOP(); }
        }
        ++g_das_uart_test_evidence.bytes_checked;
    }
    result = das_uart_rx_buffer_stop(timeout_uart);
    if (result != DAS_OK) {
        g_das_uart_test_evidence.error = UINT32_C(0x0865);
        for (;;) { __NOP(); }
    }
    g_das_uart_test_evidence.flags |= DAS_UART_FLAG_IRQ_RX;

    if (g_das_uart_test_evidence.flags != DAS_UART_REQUIRED_FLAGS) {
        g_das_uart_test_evidence.error = UINT32_C(0x0840);
        for (;;) { __NOP(); }
    }

    g_das_uart_test_evidence.booted = 1u;
    for (;;) {
        ++g_das_uart_test_evidence.heartbeat;
    }
}

/*
 * Compile/link regression: this firmware uses DAS buffered RX on USART1,
 * while USART3's IRQ remains application-owned. No duplicate strong ISR
 * symbols may be introduced by linking the optional RX backend.
 */
void USART3_IRQHandler(void) {
    __NOP();
}

void HardFault_Handler(void) {
    g_das_uart_test_evidence.error = DAS_UART_TEST_HARDFAULT;
    for (;;) { __NOP(); }
}
