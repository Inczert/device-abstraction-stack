// SPDX-License-Identifier: Apache-2.0

#include <das/board_resources.h>
#include <das/gpio.h>

#include "eth_internal.h"

#include <stddef.h>
#include <stdint.h>

#define DAS_NUCLEO_ETH_RMII_AF UINT8_C(11)

static bool eth_resource_valid(das_board_eth_resource_t resource) {
    return (unsigned)resource < (unsigned)DAS_BOARD_ETH_COUNT;
}

static const stm32h755_eth_instance_t ETH_INSTANCES[DAS_BOARD_ETH_COUNT] = {
    [DAS_BOARD_ETH_RJ45] = STM32H755_ETH1,
};

/*
 * NUCLEO-H755ZI-Q RMII route to the on-board LAN8742A:
 * PA1 REF_CLK, PA2 MDIO, PC1 MDC, PA7 CRS_DV, PC4 RXD0, PC5 RXD1,
 * PG11 TX_EN, PG13 TXD0, PB13 TXD1.
 */
static const das_gpio_pin_t ETH_RMII_PINS[] = {
    {DAS_GPIO_PORT_A, 1u},
    {DAS_GPIO_PORT_A, 2u},
    {DAS_GPIO_PORT_C, 1u},
    {DAS_GPIO_PORT_A, 7u},
    {DAS_GPIO_PORT_C, 4u},
    {DAS_GPIO_PORT_C, 5u},
    {DAS_GPIO_PORT_G, 11u},
    {DAS_GPIO_PORT_G, 13u},
    {DAS_GPIO_PORT_B, 13u},
};

static das_result_t configure_rmii_pins(void) {
    const das_gpio_config_t pin_config = {
        .mode = DAS_GPIO_MODE_ALTERNATE,
        .pull = DAS_GPIO_PULL_NONE,
        .output_type = DAS_GPIO_OUTPUT_PUSH_PULL,
        .speed = DAS_GPIO_SPEED_VERY_HIGH,
        .alternate = DAS_NUCLEO_ETH_RMII_AF,
        .initial_high = false,
    };

    for (size_t i = 0u; i < sizeof(ETH_RMII_PINS) / sizeof(ETH_RMII_PINS[0]); ++i) {
        const das_result_t result = das_gpio_configure(ETH_RMII_PINS[i], &pin_config);
        if (result != DAS_OK) {
            return result;
        }
    }
    return DAS_OK;
}

das_result_t das_board_eth_init(das_board_eth_resource_t resource,
                                const das_eth_config_t* config,
                                das_eth_t* eth) {
    if (!eth_resource_valid(resource) || config == 0 || eth == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    *eth = DAS_ETH_INVALID;
    if (!stm32h755_eth_current_core_supported()) {
        return DAS_ERROR_UNSUPPORTED;
    }

    das_result_t result = configure_rmii_pins();
    if (result != DAS_OK) {
        return result;
    }

    const das_eth_t resolved = stm32h755_eth_handle(ETH_INSTANCES[resource]);
    if (!das_eth_is_valid(resolved)) {
        return DAS_ERROR_UNSUPPORTED;
    }

    result = das_eth_init(resolved, config);
    if (result != DAS_OK) {
        return result;
    }

    *eth = resolved;
    return DAS_OK;
}
