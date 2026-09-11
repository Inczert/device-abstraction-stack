// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_STM32H755_ETH_INTERNAL_H
#define DAS_STM32H755_ETH_INTERNAL_H

#include <stdbool.h>

#include <das/eth.h>

typedef enum stm32h755_eth_instance {
    STM32H755_ETH1 = 1
} stm32h755_eth_instance_t;

/** Construct a generic DAS handle for a supported STM32H755 Ethernet MAC. */
das_eth_t stm32h755_eth_handle(stm32h755_eth_instance_t instance);

/** Return whether the current core owns the STM32H755 Ethernet baseline. */
bool stm32h755_eth_current_core_supported(void);

#endif
