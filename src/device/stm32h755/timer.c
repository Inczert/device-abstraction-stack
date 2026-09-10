// SPDX-License-Identifier: Apache-2.0

#include <das/timer.h>

#include "clock_internal.h"
#include "stm32h755xx.h"
#include "timer_internal.h"

#include <stdbool.h>
#include <stdint.h>

#define DAS_STM32H755_TIMER_GENERAL_STORAGE UINT32_C(0x544d0002)
#define DAS_STM32H755_PWM_TIM1_CH4_STORAGE  UINT32_C(0x50570104)
#define DAS_STM32H755_PWM_MAX_PERIOD_COUNTS UINT32_C(65536)

#if defined(CORE_CM7)
#define DAS_RCC_CORE RCC_C1
#elif defined(CORE_CM4)
#define DAS_RCC_CORE RCC_C2
#else
#error "STM32H755 timer backend requires CORE_CM7 or CORE_CM4"
#endif

static bool timer_valid(das_timer_t timer) {
    return timer.storage == DAS_STM32H755_TIMER_GENERAL_STORAGE;
}

static bool pwm_valid(das_pwm_t pwm) {
    return pwm.storage == DAS_STM32H755_PWM_TIM1_CH4_STORAGE;
}

static void enable_tim2_clock(void) {
    DAS_RCC_CORE->APB1LENR |= RCC_APB1LENR_TIM2EN;
    (void)DAS_RCC_CORE->APB1LENR;
    __DSB();
}

static void enable_tim1_clock(void) {
    DAS_RCC_CORE->APB2ENR |= RCC_APB2ENR_TIM1EN;
    (void)DAS_RCC_CORE->APB2ENR;
    __DSB();
}

static das_result_t timer_kernel_frequency(bool apb2, uint32_t* frequency_hz) {
    if (frequency_hz == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    stm32h755_clock_frequencies_t clocks = {0};
    const das_result_t result = stm32h755_clock_get_frequencies(0u, &clocks);
    if (result != DAS_OK) {
        return result;
    }

    const uint32_t pclk_hz = apb2 ? clocks.apb2_hz : clocks.apb1_hz;
    if (pclk_hz == 0u || clocks.ahb_hz == 0u || clocks.ahb_hz % pclk_hz != 0u) {
        return DAS_ERROR_NOT_READY;
    }

    const uint32_t apb_divider = clocks.ahb_hz / pclk_hz;
#if defined(RCC_CFGR_TIMPRE)
    if ((RCC->CFGR & RCC_CFGR_TIMPRE) == 0u) {
        *frequency_hz = apb_divider == 1u ? pclk_hz : pclk_hz * 2u;
    } else {
        *frequency_hz = apb_divider <= 4u ? clocks.ahb_hz : pclk_hz * 4u;
    }
#else
#error "STM32H755 CMSIS header does not expose RCC timer prescaler selection"
#endif
    return *frequency_hz == 0u ? DAS_ERROR_NOT_READY : DAS_OK;
}

static das_result_t periodic_dividers(uint32_t clock_hz,
                                      uint32_t requested_hz,
                                      uint32_t* prescaler,
                                      uint32_t* autoreload) {
    if (requested_hz == 0u || requested_hz > clock_hz ||
        prescaler == 0 || autoreload == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    const uint64_t period_counts =
        ((uint64_t)clock_hz + ((uint64_t)requested_hz / 2u)) /
        (uint64_t)requested_hz;
    if (period_counts == 0u || period_counts > (uint64_t)UINT32_MAX + 1u) {
        return DAS_ERROR_UNSUPPORTED;
    }

    *prescaler = 0u;
    *autoreload = (uint32_t)(period_counts - 1u);
    return DAS_OK;
}

static das_result_t pwm_dividers(uint32_t clock_hz,
                                 uint32_t requested_hz,
                                 uint32_t* prescaler,
                                 uint32_t* autoreload) {
    if (requested_hz == 0u || requested_hz > clock_hz ||
        prescaler == 0 || autoreload == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    const uint64_t total_counts =
        ((uint64_t)clock_hz + ((uint64_t)requested_hz / 2u)) /
        (uint64_t)requested_hz;
    if (total_counts == 0u) {
        return DAS_ERROR_UNSUPPORTED;
    }

    uint64_t prescaler_div =
        (total_counts + DAS_STM32H755_PWM_MAX_PERIOD_COUNTS - 1u) /
        DAS_STM32H755_PWM_MAX_PERIOD_COUNTS;
    if (prescaler_div == 0u) {
        prescaler_div = 1u;
    }
    if (prescaler_div > UINT16_MAX + UINT64_C(1)) {
        return DAS_ERROR_UNSUPPORTED;
    }

    const uint64_t denominator = (uint64_t)requested_hz * prescaler_div;
    uint64_t period_counts = ((uint64_t)clock_hz + denominator / 2u) / denominator;
    if (period_counts == 0u) {
        period_counts = 1u;
    }
    if (period_counts > DAS_STM32H755_PWM_MAX_PERIOD_COUNTS) {
        return DAS_ERROR_UNSUPPORTED;
    }

    *prescaler = (uint32_t)(prescaler_div - 1u);
    *autoreload = (uint32_t)(period_counts - 1u);
    return DAS_OK;
}

static uint32_t pwm_compare(uint32_t period_counts, uint16_t duty_per_mille) {
    uint64_t compare =
        ((uint64_t)period_counts * duty_per_mille + UINT64_C(500)) / UINT64_C(1000);
    if (compare > UINT16_MAX) {
        compare = UINT16_MAX;
    }
    return (uint32_t)compare;
}

das_result_t das_timer_init(const das_timer_config_t* config, das_timer_t* timer) {
    if (config == 0 || timer == 0 || config->frequency_hz == 0u) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    *timer = DAS_TIMER_INVALID;

    uint32_t clock_hz = 0u;
    das_result_t result = timer_kernel_frequency(false, &clock_hz);
    if (result != DAS_OK) {
        return result;
    }

    uint32_t prescaler = 0u;
    uint32_t autoreload = 0u;
    result = periodic_dividers(clock_hz, config->frequency_hz, &prescaler, &autoreload);
    if (result != DAS_OK) {
        return result;
    }

    enable_tim2_clock();
    TIM2->CR1 = 0u;
    TIM2->CR2 = 0u;
    TIM2->SMCR = 0u;
    TIM2->DIER = 0u;
    TIM2->CCER = 0u;
    TIM2->PSC = prescaler;
    TIM2->ARR = autoreload;
    TIM2->CNT = 0u;
    TIM2->SR = 0u;
    TIM2->CR1 = TIM_CR1_ARPE;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0u;
    __DSB();

    *timer = (das_timer_t){.storage = DAS_STM32H755_TIMER_GENERAL_STORAGE};
    return DAS_OK;
}

bool das_timer_is_valid(das_timer_t timer) {
    return timer_valid(timer);
}

das_result_t das_timer_start(das_timer_t timer) {
    if (!timer_valid(timer)) return DAS_ERROR_INVALID_ARGUMENT;
    TIM2->CR1 |= TIM_CR1_CEN;
    return DAS_OK;
}

das_result_t das_timer_stop(das_timer_t timer) {
    if (!timer_valid(timer)) return DAS_ERROR_INVALID_ARGUMENT;
    TIM2->CR1 &= ~TIM_CR1_CEN;
    return DAS_OK;
}

das_result_t das_timer_is_running(das_timer_t timer, bool* running) {
    if (!timer_valid(timer) || running == 0) return DAS_ERROR_INVALID_ARGUMENT;
    *running = (TIM2->CR1 & TIM_CR1_CEN) != 0u;
    return DAS_OK;
}

das_result_t das_timer_get_frequency(das_timer_t timer, uint32_t* frequency_hz) {
    if (!timer_valid(timer) || frequency_hz == 0) return DAS_ERROR_INVALID_ARGUMENT;

    uint32_t clock_hz = 0u;
    const das_result_t result = timer_kernel_frequency(false, &clock_hz);
    if (result != DAS_OK) return result;

    const uint64_t divisor =
        ((uint64_t)TIM2->PSC + 1u) * ((uint64_t)TIM2->ARR + 1u);
    if (divisor == 0u) return DAS_ERROR_NOT_READY;
    *frequency_hz = (uint32_t)(((uint64_t)clock_hz + divisor / 2u) / divisor);
    return DAS_OK;
}

das_result_t das_timer_get_counter(das_timer_t timer, uint32_t* counter) {
    if (!timer_valid(timer) || counter == 0) return DAS_ERROR_INVALID_ARGUMENT;
    *counter = TIM2->CNT;
    return DAS_OK;
}

das_result_t das_timer_reset_counter(das_timer_t timer) {
    if (!timer_valid(timer)) return DAS_ERROR_INVALID_ARGUMENT;
    TIM2->CNT = 0u;
    return DAS_OK;
}

das_result_t das_timer_update_interrupt_enable(das_timer_t timer, bool enabled) {
    if (!timer_valid(timer)) return DAS_ERROR_INVALID_ARGUMENT;
    if (enabled) TIM2->DIER |= TIM_DIER_UIE;
    else TIM2->DIER &= ~TIM_DIER_UIE;
    return DAS_OK;
}

das_result_t das_timer_update_pending(das_timer_t timer, bool* pending) {
    if (!timer_valid(timer) || pending == 0) return DAS_ERROR_INVALID_ARGUMENT;
    *pending = (TIM2->SR & TIM_SR_UIF) != 0u;
    return DAS_OK;
}

das_result_t das_timer_clear_update(das_timer_t timer) {
    if (!timer_valid(timer)) return DAS_ERROR_INVALID_ARGUMENT;
    TIM2->SR &= ~TIM_SR_UIF;
    return DAS_OK;
}

das_result_t das_timer_get_irq(das_timer_t timer, das_irq_t* irq) {
    if (!timer_valid(timer) || irq == 0) return DAS_ERROR_INVALID_ARGUMENT;
    irq->storage = (uint32_t)(int32_t)TIM2_IRQn;
    return DAS_OK;
}

das_pwm_t stm32h755_pwm_handle(stm32h755_pwm_output_t output) {
    if (output != STM32H755_PWM_TIM1_CH4) return DAS_PWM_INVALID;
    return (das_pwm_t){.storage = DAS_STM32H755_PWM_TIM1_CH4_STORAGE};
}

bool das_pwm_is_valid(das_pwm_t pwm) {
    return pwm_valid(pwm);
}

das_result_t das_pwm_init(das_pwm_t pwm, const das_pwm_config_t* config) {
    if (!pwm_valid(pwm) || config == 0 || config->frequency_hz == 0u ||
        config->duty_per_mille > 1000u) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    uint32_t clock_hz = 0u;
    das_result_t result = timer_kernel_frequency(true, &clock_hz);
    if (result != DAS_OK) return result;

    uint32_t prescaler = 0u;
    uint32_t autoreload = 0u;
    result = pwm_dividers(clock_hz, config->frequency_hz, &prescaler, &autoreload);
    if (result != DAS_OK) return result;

    enable_tim1_clock();
    TIM1->CR1 = 0u;
    TIM1->CR2 = 0u;
    TIM1->SMCR = 0u;
    TIM1->DIER = 0u;
    TIM1->CCER = 0u;
    TIM1->PSC = prescaler;
    TIM1->ARR = autoreload;
    TIM1->CNT = 0u;

    TIM1->CCMR2 &= ~(TIM_CCMR2_CC4S | TIM_CCMR2_OC4M);
    TIM1->CCMR2 |= TIM_CCMR2_OC4PE | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2;
    TIM1->CCR4 = pwm_compare(autoreload + 1u, config->duty_per_mille);
    TIM1->CCER &= ~(TIM_CCER_CC4P | TIM_CCER_CC4NP);
    TIM1->BDTR |= TIM_BDTR_MOE;
    TIM1->CR1 = TIM_CR1_ARPE;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->SR = 0u;
    __DSB();
    return DAS_OK;
}

das_result_t das_pwm_start(das_pwm_t pwm) {
    if (!pwm_valid(pwm)) return DAS_ERROR_INVALID_ARGUMENT;
    TIM1->BDTR |= TIM_BDTR_MOE;
    TIM1->CCER |= TIM_CCER_CC4E;
    TIM1->CR1 |= TIM_CR1_CEN;
    return DAS_OK;
}

das_result_t das_pwm_stop(das_pwm_t pwm) {
    if (!pwm_valid(pwm)) return DAS_ERROR_INVALID_ARGUMENT;
    TIM1->CR1 &= ~TIM_CR1_CEN;
    TIM1->CCER &= ~TIM_CCER_CC4E;
    return DAS_OK;
}

das_result_t das_pwm_is_running(das_pwm_t pwm, bool* running) {
    if (!pwm_valid(pwm) || running == 0) return DAS_ERROR_INVALID_ARGUMENT;
    *running = (TIM1->CR1 & TIM_CR1_CEN) != 0u &&
               (TIM1->CCER & TIM_CCER_CC4E) != 0u;
    return DAS_OK;
}

das_result_t das_pwm_set_duty(das_pwm_t pwm, uint16_t duty_per_mille) {
    if (!pwm_valid(pwm) || duty_per_mille > 1000u) return DAS_ERROR_INVALID_ARGUMENT;
    const uint32_t period_counts = TIM1->ARR + 1u;
    if (period_counts == 0u) return DAS_ERROR_NOT_READY;
    TIM1->CCR4 = pwm_compare(period_counts, duty_per_mille);
    if ((TIM1->CR1 & TIM_CR1_CEN) == 0u) TIM1->EGR = TIM_EGR_UG;
    return DAS_OK;
}

das_result_t das_pwm_get_duty(das_pwm_t pwm, uint16_t* duty_per_mille) {
    if (!pwm_valid(pwm) || duty_per_mille == 0) return DAS_ERROR_INVALID_ARGUMENT;
    const uint32_t period_counts = TIM1->ARR + 1u;
    if (period_counts == 0u) return DAS_ERROR_NOT_READY;
    uint32_t duty = (uint32_t)(((uint64_t)TIM1->CCR4 * UINT64_C(1000) +
                                period_counts / 2u) /
                               period_counts);
    if (duty > 1000u) duty = 1000u;
    *duty_per_mille = (uint16_t)duty;
    return DAS_OK;
}

das_result_t das_pwm_get_frequency(das_pwm_t pwm, uint32_t* frequency_hz) {
    if (!pwm_valid(pwm) || frequency_hz == 0) return DAS_ERROR_INVALID_ARGUMENT;

    uint32_t clock_hz = 0u;
    const das_result_t result = timer_kernel_frequency(true, &clock_hz);
    if (result != DAS_OK) return result;

    const uint64_t divisor =
        ((uint64_t)TIM1->PSC + 1u) * ((uint64_t)TIM1->ARR + 1u);
    if (divisor == 0u) return DAS_ERROR_NOT_READY;
    *frequency_hz = (uint32_t)(((uint64_t)clock_hz + divisor / 2u) / divisor);
    return DAS_OK;
}
