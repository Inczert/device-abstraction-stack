// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_STM32H755_POWER_INTERNAL_H
#define DAS_STM32H755_POWER_INTERNAL_H

#include <stdint.h>

#include <das/result.h>

/**
 * Configure the STM32H755 core supply for direct-SMPS operation.
 *
 * This is device-internal plumbing. Whether direct SMPS matches the physical
 * board wiring is board policy. CPU1/CM7 owns supply reconfiguration.
 */
das_result_t stm32h755_power_configure_direct_smps(uint32_t wait_limit);

#endif
