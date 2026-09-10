// SPDX-License-Identifier: Apache-2.0

#include <das/board_resources.h>

#include "eth_internal.h"

static bool eth_resource_valid(das_board_eth_resource_t resource) {
    return (unsigned)resource < (unsigned)DAS_BOARD_ETH_COUNT;
}

static const stm32h755_eth_instance_t ETH_INSTANCES[DAS_BOARD_ETH_COUNT] = {
    [DAS_BOARD_ETH_RJ45] = STM32H755_ETH1,
};

das_result_t das_board_eth_init(das_board_eth_resource_t resource,
                                const das_eth_config_t* config,
                                das_eth_t* eth) {
    if (!eth_resource_valid(resource) || config == 0 || eth == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    *eth = DAS_ETH_INVALID;
    const das_eth_t resolved = stm32h755_eth_handle(ETH_INSTANCES[resource]);
    if (!das_eth_is_valid(resolved)) {
        return DAS_ERROR_UNSUPPORTED;
    }

    const das_result_t result = das_eth_init(resolved, config);
    if (result != DAS_OK) {
        return result;
    }

    *eth = resolved;
    return DAS_OK;
}
