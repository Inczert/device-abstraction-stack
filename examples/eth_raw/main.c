// SPDX-License-Identifier: Apache-2.0

#include <das/das.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static das_eth_t g_eth = DAS_ETH_INVALID;
static uint8_t g_rx_buffer[DAS_ETH_MAX_FRAME_SIZE];

volatile uint32_t g_das_eth_link_up;
volatile uint32_t g_das_eth_speed_mbps;
volatile uint32_t g_das_eth_duplex;
volatile uint32_t g_das_eth_tx_count;
volatile uint32_t g_das_eth_rx_count;
volatile uint32_t g_das_eth_rx_bytes;
volatile uint32_t g_das_eth_rx_test_count;
volatile uint32_t g_das_eth_rx_test_errors;
volatile uint32_t g_das_eth_rx_last_sequence;
volatile int32_t g_das_eth_last_result;

static const das_eth_config_t ETH_CONFIG = {
    .mac = {0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u},
};

/* Broadcast destination, DAS source MAC, private experimental EtherType 0x88B5. */
static const uint8_t TEST_FRAME[60] = {
    0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu,
    0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u,
    0x88u, 0xb5u,
    'D', 'A', 'S', ' ', 'E', 'T', 'H', ' ', 'L', '2',
    ' ', 't', 'e', 's', 't',
};

enum {
    ETH_HEADER_SIZE = 14u,
    RX_TEST_FRAME_SIZE = 60u,
    RX_TEST_MAGIC_SIZE = 8u,
    RX_TEST_SEQUENCE_OFFSET = ETH_HEADER_SIZE + RX_TEST_MAGIC_SIZE,
    RX_TEST_PATTERN_OFFSET = RX_TEST_SEQUENCE_OFFSET + 4u,
};

static const uint8_t RX_TEST_MAGIC[RX_TEST_MAGIC_SIZE] = {
    'D', 'A', 'S', 'R', 'X', 'V', '1', 0u
};

static bool is_rx_test_candidate(const uint8_t* frame, size_t length) {
    return length >= ETH_HEADER_SIZE &&
           memcmp(frame, ETH_CONFIG.mac, DAS_ETH_MAC_ADDRESS_SIZE) == 0 &&
           frame[12] == 0x88u && frame[13] == 0xb6u;
}

static bool validate_rx_test_frame(const uint8_t* frame,
                                   size_t length,
                                   uint32_t* out_sequence) {
    uint32_t sequence;
    size_t index;

    if (frame == NULL || out_sequence == NULL ||
        length != RX_TEST_FRAME_SIZE ||
        memcmp(frame + ETH_HEADER_SIZE, RX_TEST_MAGIC, RX_TEST_MAGIC_SIZE) != 0) {
        return false;
    }

    sequence = ((uint32_t)frame[RX_TEST_SEQUENCE_OFFSET] << 24u) |
               ((uint32_t)frame[RX_TEST_SEQUENCE_OFFSET + 1u] << 16u) |
               ((uint32_t)frame[RX_TEST_SEQUENCE_OFFSET + 2u] << 8u) |
               (uint32_t)frame[RX_TEST_SEQUENCE_OFFSET + 3u];

    for (index = RX_TEST_PATTERN_OFFSET; index < RX_TEST_FRAME_SIZE; ++index) {
        const uint8_t expected = (uint8_t)(
            (sequence + (uint32_t)(index - RX_TEST_PATTERN_OFFSET)) & UINT32_C(0xff));
        if (frame[index] != expected) {
            return false;
        }
    }

    *out_sequence = sequence;
    return true;
}

static int das_init(void) {
    if (das_board_led_init_all(false) != DAS_OK) {
        return 1;
    }
    if (das_clock_set_frequency(UINT32_C(400000000)) != DAS_OK) {
        return 2;
    }
    if (das_time_init() != DAS_OK) {
        return 3;
    }
    if (das_board_eth_init(DAS_BOARD_ETH_RJ45, &ETH_CONFIG, &g_eth) != DAS_OK) {
        return 4;
    }
    return 0;
}

static void fail(int32_t result) {
    g_das_eth_last_result = result;
    (void)das_board_led_set(DAS_BOARD_LED_RED, true);
}

int main(void) {
    const int init_result = das_init();
    if (init_result != 0) {
        fail(-init_result);
        for (;;) {
        }
    }

    das_time_ms_t last_tx = das_time_now_ms();
    bool previous_link = false;

    for (;;) {
        das_eth_link_state_t link = {0};
        das_result_t result = das_eth_link_state(g_eth, &link);
        g_das_eth_last_result = result;
        if (result != DAS_OK) {
            fail(result);
            (void)das_delay_ms(100u);
            continue;
        }

        g_das_eth_link_up = link.up ? 1u : 0u;
        g_das_eth_speed_mbps = link.speed_mbps;
        g_das_eth_duplex = (uint32_t)link.duplex;
        if (link.up != previous_link) {
            (void)das_board_led_set(DAS_BOARD_LED_GREEN, link.up);
            previous_link = link.up;
        }

        if (link.up && das_time_interval_elapsed(last_tx, 1000u)) {
            last_tx = das_time_now_ms();
            result = das_eth_send(g_eth, TEST_FRAME, sizeof(TEST_FRAME));
            g_das_eth_last_result = result;
            if (result == DAS_OK) {
                ++g_das_eth_tx_count;
                (void)das_board_led_toggle(DAS_BOARD_LED_YELLOW);
            } else {
                fail(result);
            }
        }

        for (;;) {
            size_t received = 0u;
            result = das_eth_receive(g_eth,
                                     g_rx_buffer,
                                     sizeof(g_rx_buffer),
                                     &received);
            g_das_eth_last_result = result;
            if (result != DAS_OK) {
                fail(result);
                break;
            }
            if (received == 0u) {
                break;
            }

            ++g_das_eth_rx_count;
            g_das_eth_rx_bytes += (uint32_t)received;

            if (is_rx_test_candidate(g_rx_buffer, received)) {
                uint32_t sequence = 0u;
                if (validate_rx_test_frame(g_rx_buffer, received, &sequence)) {
                    ++g_das_eth_rx_test_count;
                    g_das_eth_rx_last_sequence = sequence;
                } else {
                    ++g_das_eth_rx_test_errors;
                }
            }
        }

        (void)das_delay_ms(10u);
    }
}
