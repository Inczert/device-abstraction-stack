// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_TIMER_H
#define DAS_TIMER_H

#include <stdbool.h>
#include <stdint.h>

#include <das/irq.h>
#include <das/result.h>

/** Opaque periodic-timer handle. storage is backend-owned. */
typedef struct das_timer {
    uint32_t storage;
} das_timer_t;

#define DAS_TIMER_INVALID ((das_timer_t){UINT32_MAX})

/**
 * Periodic timer configuration.
 *
 * frequency_hz is the requested update/event frequency. The backend selects
 * prescaler/period values and exposes the effective frequency separately.
 */
typedef struct das_timer_config {
    uint32_t frequency_hz;
} das_timer_config_t;

/** Configure the backend's general-purpose periodic timer and return its handle. */
das_result_t das_timer_init(const das_timer_config_t* config, das_timer_t* timer);

/** Return true when the active backend recognizes the handle. */
bool das_timer_is_valid(das_timer_t timer);

/** Start/stop counter execution. */
das_result_t das_timer_start(das_timer_t timer);
das_result_t das_timer_stop(das_timer_t timer);
das_result_t das_timer_is_running(das_timer_t timer, bool* running);

/** Return the effective periodic update frequency in hertz. */
das_result_t das_timer_get_frequency(das_timer_t timer, uint32_t* frequency_hz);

/** Read/reset the live timer counter value. */
das_result_t das_timer_get_counter(das_timer_t timer, uint32_t* counter);
das_result_t das_timer_reset_counter(das_timer_t timer);

/** Timer update-event source control. */
das_result_t das_timer_update_interrupt_enable(das_timer_t timer, bool enabled);
das_result_t das_timer_update_pending(das_timer_t timer, bool* pending);
das_result_t das_timer_clear_update(das_timer_t timer);

/** Resolve the interrupt-controller line used by the timer update source. */
das_result_t das_timer_get_irq(das_timer_t timer, das_irq_t* irq);

/** Opaque PWM output handle. storage is backend-owned. */
typedef struct das_pwm {
    uint32_t storage;
} das_pwm_t;

#define DAS_PWM_INVALID ((das_pwm_t){UINT32_MAX})

/** PWM configuration. duty_per_mille is 0..1000 inclusive. */
typedef struct das_pwm_config {
    uint32_t frequency_hz;
    uint16_t duty_per_mille;
} das_pwm_config_t;

bool das_pwm_is_valid(das_pwm_t pwm);
das_result_t das_pwm_init(das_pwm_t pwm, const das_pwm_config_t* config);
das_result_t das_pwm_start(das_pwm_t pwm);
das_result_t das_pwm_stop(das_pwm_t pwm);
das_result_t das_pwm_is_running(das_pwm_t pwm, bool* running);
das_result_t das_pwm_set_duty(das_pwm_t pwm, uint16_t duty_per_mille);
das_result_t das_pwm_get_duty(das_pwm_t pwm, uint16_t* duty_per_mille);
das_result_t das_pwm_get_frequency(das_pwm_t pwm, uint32_t* frequency_hz);

#endif
