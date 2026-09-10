// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_STM32H755_SPI_INTERNAL_H
#define DAS_STM32H755_SPI_INTERNAL_H

#include <das/spi.h>

typedef enum stm32h755_spi_instance {
    STM32H755_SPI1 = 0
} stm32h755_spi_instance_t;

das_spi_t stm32h755_spi_handle(stm32h755_spi_instance_t instance);

#endif
