// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_STM32H755_I2C_INTERNAL_H
#define DAS_STM32H755_I2C_INTERNAL_H

#include <das/i2c.h>

typedef enum stm32h755_i2c_instance {
    STM32H755_I2C1 = 0
} stm32h755_i2c_instance_t;

das_i2c_t stm32h755_i2c_handle(stm32h755_i2c_instance_t instance);

#endif
