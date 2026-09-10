// SPDX-License-Identifier: Apache-2.0

#include <das/eth.h>

#include "eth_internal.h"

#include <stddef.h>
#include <stdint.h>

#define STM32H755_ETH_HANDLE_BASE UINT32_C(0x45544800)
#define STM32H755_ETH_HANDLE_MASK UINT32_C(0xffffff00)
#define STM32H755_ETH_INSTANCE_MASK UINT32_C(0x000000ff)

static bool mac_address_valid(const uint8_t mac[DAS_ETH_MAC_ADDRESS_SIZE]) {
    if ((mac[0] & UINT8_C(0x01)) != 0u) {
        return false;
    }

    uint8_t combined = 0u;
    for (size_t i = 0u; i < DAS_ETH_MAC_ADDRESS_SIZE; ++i) {
        combined |= mac[i];
    }
    return combined != 0u;
}

static stm32h755_eth_instance_t instance_from_handle(das_eth_t eth) {
    if ((eth.storage & STM32H755_ETH_HANDLE_MASK) != STM32H755_ETH_HANDLE_BASE) {
        return (stm32h755_eth_instance_t)0;
    }
    return (stm32h755_eth_instance_t)(eth.storage & STM32H755_ETH_INSTANCE_MASK);
}

das_eth_t stm32h755_eth_handle(stm32h755_eth_instance_t instance) {
    if (instance != STM32H755_ETH1) {
        return DAS_ETH_INVALID;
    }
    return (das_eth_t){STM32H755_ETH_HANDLE_BASE | (uint32_t)instance};
}

bool das_eth_is_valid(das_eth_t eth) {
    return instance_from_handle(eth) == STM32H755_ETH1;
}

das_result_t das_eth_init(das_eth_t eth, const das_eth_config_t* config) {
    if (!das_eth_is_valid(eth) || config == 0 || !mac_address_valid(config->mac)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    /*
     * Public/board contract is established first. The STM32H755 MAC DMA,
     * descriptor rings, RMII/MDIO and LAN8742A PHY implementation lands in the
     * next Ethernet backend step rather than pretending a partial MAC is ready.
     */
    return DAS_ERROR_UNSUPPORTED;
}

das_result_t das_eth_send(das_eth_t eth, const uint8_t* frame, size_t length) {
    if (!das_eth_is_valid(eth) || frame == 0 || length == 0u) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    return DAS_ERROR_UNSUPPORTED;
}

das_result_t das_eth_receive(das_eth_t eth,
                             uint8_t* buffer,
                             size_t capacity,
                             size_t* received) {
    if (!das_eth_is_valid(eth) || buffer == 0 || capacity == 0u || received == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    *received = 0u;
    return DAS_ERROR_UNSUPPORTED;
}

das_result_t das_eth_link_state(das_eth_t eth, das_eth_link_state_t* state) {
    if (!das_eth_is_valid(eth) || state == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    state->up = false;
    state->speed_mbps = 0u;
    state->duplex = DAS_ETH_DUPLEX_UNKNOWN;
    return DAS_ERROR_UNSUPPORTED;
}
