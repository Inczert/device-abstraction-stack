// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_STM32H755_TIMER_INTERNAL_H
#define DAS_STM32H755_TIMER_INTERNAL_H

#include <stdint.h>

#include <das/timer.h>

typedef enum stm32h755_pwm_output {
    STM32H755_PWM_TIM1_CH4 = 0
} stm32h755_pwm_output_t;

/** Resolve a device-private PWM output into the generic opaque handle. */
das_pwm_t stm32h755_pwm_handle(stm32h755_pwm_output_t output);

#endif
