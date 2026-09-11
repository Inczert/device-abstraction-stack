// SPDX-License-Identifier: Apache-2.0

#include <das/das.h>

#include <stdbool.h>
#include <stdint.h>

static das_eth_t g_eth = DAS_ETH_INVALID;
static uint8_t g_rx_buffer[DAS_ETH_MAX_FRAME_SIZE];

volatile uint32_t g_das_eth_link_up;
volatile uint32_t g_das_eth_speed_mbps;
volatile uint32_t g_das_eth_duplex;
volatile uint32_t g_das_eth_tx_count;
volatile uint32_t g_das_eth_rx_count;
volatile uint32_t g_das_eth_rx_bytes;
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
        }

        (void)das_delay_ms(10u);
    }
}
