// SPDX-License-Identifier: Apache-2.0

#include <das/board_resources.h>
#include <das/clock.h>
#include <das/cortex_m/startup.h>
#include <das/gpio.h>
#include <das/spi.h>
#include <das/time.h>

#include "stm32h755xx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DAS_SPI_TEST_MAGIC UINT32_C(0x44535049)
#define DAS_SPI_TEST_HARDFAULT UINT32_C(0xe00b0001)

#define DAS_SPI_FLAG_MAPPING   (UINT32_C(1) << 0u)
#define DAS_SPI_FLAG_CS        (UINT32_C(1) << 1u)
#define DAS_SPI_FLAG_MODE0     (UINT32_C(1) << 2u)
#define DAS_SPI_FLAG_MODE1     (UINT32_C(1) << 3u)
#define DAS_SPI_FLAG_MODE2     (UINT32_C(1) << 4u)
#define DAS_SPI_FLAG_MODE3     (UINT32_C(1) << 5u)
#define DAS_SPI_FLAG_RX_ONLY   (UINT32_C(1) << 6u)
#define DAS_SPI_FLAG_TX_ONLY   (UINT32_C(1) << 7u)
#define DAS_SPI_FLAG_HANDLE    (UINT32_C(1) << 8u)
#define DAS_SPI_REQUIRED_FLAGS UINT32_C(0x1ff)

typedef struct das_spi_test_evidence {
    uint32_t magic;
    volatile uint32_t booted;
    volatile uint32_t error;
    volatile uint32_t heartbeat;
    volatile int32_t clock_result;
    volatile int32_t core_clock_result;
    volatile int32_t time_result;
    volatile uint32_t core_hz;
    volatile uint32_t flags;
    volatile uint32_t hz_mode0;
    volatile uint32_t hz_mode1;
    volatile uint32_t hz_mode2;
    volatile uint32_t hz_mode3;
    volatile uint32_t bytes_checked;
    volatile uint32_t cs_selected;
    volatile uint32_t cs_inactive;
    volatile uint32_t mismatch_expected;
    volatile uint32_t mismatch_actual;
} das_spi_test_evidence_t;

volatile das_spi_test_evidence_t g_das_spi_test_evidence = {
    .magic = DAS_SPI_TEST_MAGIC,
};

static bool pin_equal(das_gpio_pin_t pin, das_gpio_port_t port, uint8_t number) {
    return pin.port == port && pin.pin == number;
}

static void make_pattern(uint8_t* data, size_t size, uint8_t seed) {
    for (size_t index = 0u; index < size; ++index) {
        data[index] = (uint8_t)(((uint32_t)index * UINT32_C(37) + seed) & UINT32_C(0xff));
    }
}

static das_result_t open_spi(const das_spi_config_t* config,
                             das_spi_t* spi,
                             uint32_t* effective_hz) {
    das_result_t result = das_board_spi_init(DAS_BOARD_SPI_ARDUINO, config, spi);
    if (result != DAS_OK) return result;
    if (!das_spi_is_valid(*spi)) return DAS_ERROR_NOT_READY;
    g_das_spi_test_evidence.flags |= DAS_SPI_FLAG_HANDLE;

    result = das_spi_get_frequency(*spi, effective_hz);
    if (result != DAS_OK) return result;
    if (*effective_hz == 0u || *effective_hz > config->frequency_hz) {
        return DAS_ERROR_IO;
    }
    return DAS_OK;
}

static das_result_t run_loopback(const das_spi_config_t* config,
                                 size_t size,
                                 uint8_t seed,
                                 uint32_t* effective_hz) {
    uint8_t tx[64];
    uint8_t rx[64];
    if (size > sizeof(tx)) return DAS_ERROR_INVALID_ARGUMENT;

    make_pattern(tx, size, seed);
    for (size_t index = 0u; index < size; ++index) rx[index] = 0u;

    das_spi_t spi = DAS_SPI_INVALID;
    das_result_t result = open_spi(config, &spi, effective_hz);
    if (result != DAS_OK) return result;

    result = das_board_spi_chip_select(DAS_BOARD_SPI_ARDUINO, true);
    if (result != DAS_OK) return result;
    result = das_spi_transfer_timeout(spi, tx, rx, size, 100u);
    const das_result_t deselect =
        das_board_spi_chip_select(DAS_BOARD_SPI_ARDUINO, false);
    if (result != DAS_OK) return result;
    if (deselect != DAS_OK) return deselect;

    for (size_t index = 0u; index < size; ++index) {
        ++g_das_spi_test_evidence.bytes_checked;
        if (rx[index] != tx[index]) {
            g_das_spi_test_evidence.mismatch_expected = tx[index];
            g_das_spi_test_evidence.mismatch_actual = rx[index];
            return DAS_ERROR_IO;
        }
    }
    return DAS_OK;
}

int main(void) {
#if defined(CORE_CM7)
    g_das_spi_test_evidence.clock_result = das_clock_set_frequency(UINT32_C(400000000));
#else
    g_das_spi_test_evidence.clock_result = DAS_OK;
#endif
    if (g_das_spi_test_evidence.clock_result != DAS_OK) {
        g_das_spi_test_evidence.error = UINT32_C(0x0b01);
        for (;;) { __NOP(); }
    }

    uint32_t core_hz = 0u;
    g_das_spi_test_evidence.core_clock_result = das_clock_get_core_frequency(&core_hz);
    g_das_spi_test_evidence.core_hz = core_hz;
    if (g_das_spi_test_evidence.core_clock_result != DAS_OK) {
        g_das_spi_test_evidence.error = UINT32_C(0x0b02);
        for (;;) { __NOP(); }
    }

    g_das_spi_test_evidence.time_result = das_time_init();
    if (g_das_spi_test_evidence.time_result != DAS_OK) {
        g_das_spi_test_evidence.error = UINT32_C(0x0b03);
        for (;;) { __NOP(); }
    }

    das_board_spi_pins_t pins;
    das_result_t result = das_board_spi_get_pins(DAS_BOARD_SPI_ARDUINO, &pins);
    if (result != DAS_OK ||
        !pin_equal(pins.sck, DAS_GPIO_PORT_A, 5u) ||
        !pin_equal(pins.miso, DAS_GPIO_PORT_A, 6u) ||
        !pin_equal(pins.mosi, DAS_GPIO_PORT_B, 5u) ||
        !pin_equal(pins.cs, DAS_GPIO_PORT_D, 14u)) {
        g_das_spi_test_evidence.error = UINT32_C(0x0b04);
        for (;;) { __NOP(); }
    }
    g_das_spi_test_evidence.flags |= DAS_SPI_FLAG_MAPPING;

    const das_spi_config_t config0 = {
        .frequency_hz = UINT32_C(1000000),
        .mode = DAS_SPI_MODE_0,
        .bit_order = DAS_SPI_MSB_FIRST,
    };
    das_spi_t cs_spi = DAS_SPI_INVALID;
    uint32_t ignored_hz = 0u;
    result = open_spi(&config0, &cs_spi, &ignored_hz);
    if (result != DAS_OK) {
        g_das_spi_test_evidence.error = UINT32_C(0x0b05);
        for (;;) { __NOP(); }
    }
    result = das_board_spi_chip_select(DAS_BOARD_SPI_ARDUINO, true);
    g_das_spi_test_evidence.cs_selected = !das_gpio_read_output(pins.cs);
    if (result != DAS_OK || g_das_spi_test_evidence.cs_selected == 0u) {
        g_das_spi_test_evidence.error = UINT32_C(0x0b06);
        for (;;) { __NOP(); }
    }
    result = das_board_spi_chip_select(DAS_BOARD_SPI_ARDUINO, false);
    g_das_spi_test_evidence.cs_inactive = das_gpio_read_output(pins.cs);
    if (result != DAS_OK || g_das_spi_test_evidence.cs_inactive == 0u) {
        g_das_spi_test_evidence.error = UINT32_C(0x0b07);
        for (;;) { __NOP(); }
    }
    g_das_spi_test_evidence.flags |= DAS_SPI_FLAG_CS;

    result = run_loopback(&config0, 1u, UINT8_C(0x5a),
                          (uint32_t*)&g_das_spi_test_evidence.hz_mode0);
    if (result != DAS_OK || g_das_spi_test_evidence.hz_mode0 != UINT32_C(1000000)) {
        g_das_spi_test_evidence.error = UINT32_C(0x0b10) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    g_das_spi_test_evidence.flags |= DAS_SPI_FLAG_MODE0;

    const das_spi_config_t config1 = {
        .frequency_hz = UINT32_C(2000000),
        .mode = DAS_SPI_MODE_1,
        .bit_order = DAS_SPI_LSB_FIRST,
    };
    result = run_loopback(&config1, 7u, UINT8_C(0x21),
                          (uint32_t*)&g_das_spi_test_evidence.hz_mode1);
    if (result != DAS_OK || g_das_spi_test_evidence.hz_mode1 != UINT32_C(2000000)) {
        g_das_spi_test_evidence.error = UINT32_C(0x0b20) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    g_das_spi_test_evidence.flags |= DAS_SPI_FLAG_MODE1;

    const das_spi_config_t config2 = {
        .frequency_hz = UINT32_C(4000000),
        .mode = DAS_SPI_MODE_2,
        .bit_order = DAS_SPI_MSB_FIRST,
    };
    result = run_loopback(&config2, 31u, UINT8_C(0xa7),
                          (uint32_t*)&g_das_spi_test_evidence.hz_mode2);
    if (result != DAS_OK || g_das_spi_test_evidence.hz_mode2 != UINT32_C(4000000)) {
        g_das_spi_test_evidence.error = UINT32_C(0x0b30) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    g_das_spi_test_evidence.flags |= DAS_SPI_FLAG_MODE2;

    const das_spi_config_t config3 = {
        .frequency_hz = UINT32_C(8000000),
        .mode = DAS_SPI_MODE_3,
        .bit_order = DAS_SPI_LSB_FIRST,
    };
    result = run_loopback(&config3, 64u, UINT8_C(0x3c),
                          (uint32_t*)&g_das_spi_test_evidence.hz_mode3);
    if (result != DAS_OK || g_das_spi_test_evidence.hz_mode3 != UINT32_C(8000000)) {
        g_das_spi_test_evidence.error = UINT32_C(0x0b40) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    g_das_spi_test_evidence.flags |= DAS_SPI_FLAG_MODE3;

    das_spi_t spi = DAS_SPI_INVALID;
    ignored_hz = 0u;
    result = open_spi(&config0, &spi, &ignored_hz);
    if (result != DAS_OK) {
        g_das_spi_test_evidence.error = UINT32_C(0x0b50);
        for (;;) { __NOP(); }
    }

    uint8_t rx_fill[8] = {0};
    result = das_spi_transfer_timeout(spi, 0, rx_fill, sizeof(rx_fill), 100u);
    if (result != DAS_OK) {
        g_das_spi_test_evidence.error = UINT32_C(0x0b51) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    for (size_t index = 0u; index < sizeof(rx_fill); ++index) {
        ++g_das_spi_test_evidence.bytes_checked;
        if (rx_fill[index] != UINT8_C(0xff)) {
            g_das_spi_test_evidence.mismatch_expected = UINT32_C(0xff);
            g_das_spi_test_evidence.mismatch_actual = rx_fill[index];
            g_das_spi_test_evidence.error = UINT32_C(0x0b52);
            for (;;) { __NOP(); }
        }
    }
    g_das_spi_test_evidence.flags |= DAS_SPI_FLAG_RX_ONLY;

    uint8_t tx_only[16];
    make_pattern(tx_only, sizeof(tx_only), UINT8_C(0xd3));
    result = das_spi_transfer_timeout(spi, tx_only, 0, sizeof(tx_only), 100u);
    if (result != DAS_OK) {
        g_das_spi_test_evidence.error = UINT32_C(0x0b53) | (uint32_t)(-result & 0x0f);
        for (;;) { __NOP(); }
    }
    g_das_spi_test_evidence.flags |= DAS_SPI_FLAG_TX_ONLY;

    if (g_das_spi_test_evidence.flags != DAS_SPI_REQUIRED_FLAGS ||
        g_das_spi_test_evidence.bytes_checked != UINT32_C(111)) {
        g_das_spi_test_evidence.error = UINT32_C(0x0b60);
        for (;;) { __NOP(); }
    }

    g_das_spi_test_evidence.booted = 1u;
    for (;;) {
        ++g_das_spi_test_evidence.heartbeat;
    }
}

void HardFault_Handler(void) {
    g_das_spi_test_evidence.error = DAS_SPI_TEST_HARDFAULT;
    for (;;) { __NOP(); }
}
