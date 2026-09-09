// SPDX-License-Identifier: Apache-2.0

#include "clock_internal.h"
#include "stm32h755xx.h"

#include <stdbool.h>
#include <stdint.h>

#define STM32H755_HSI_HZ UINT32_C(64000000)
#define STM32H755_CSI_HZ UINT32_C(4000000)
#define STM32H755_HSE_MIN_HZ UINT32_C(4000000)
#define STM32H755_HSE_MAX_HZ UINT32_C(48000000)
#define STM32H755_MANAGED_CM7_MAX_HZ UINT32_C(400000000)
#define STM32H755_MANAGED_AHB_MAX_HZ UINT32_C(200000000)
#define STM32H755_MANAGED_APB_MAX_HZ UINT32_C(100000000)
#define STM32H755_PLL_WIDE_VCO_MIN_HZ UINT64_C(192000000)
#define STM32H755_PLL_WIDE_VCO_MAX_HZ UINT64_C(836000000)
#define STM32H755_MANAGED_FLASH_LATENCY FLASH_ACR_LATENCY_4WS
#define STM32H755_MANAGED_FLASH_PROGRAM_DELAY FLASH_ACR_WRHIGHFREQ_1
/* STM32H755 VOS1 encodes both VOS bits set. */
#define STM32H755_MANAGED_VOS (PWR_D3CR_VOS_1 | PWR_D3CR_VOS_0)

static uint32_t hsi_frequency_hz(void) {
    switch (RCC->CR & RCC_CR_HSIDIV) {
        case RCC_CR_HSIDIV_2: return STM32H755_HSI_HZ / 2u;
        case RCC_CR_HSIDIV_4: return STM32H755_HSI_HZ / 4u;
        case RCC_CR_HSIDIV_8: return STM32H755_HSI_HZ / 8u;
        default: return STM32H755_HSI_HZ;
    }
}

static uint32_t core_ahb_divider_from_bits(uint32_t bits) {
    if ((bits & 0x8u) == 0u) return 1u;
    switch (bits & 0xfu) {
        case 8u: return 2u;
        case 9u: return 4u;
        case 10u: return 8u;
        case 11u: return 16u;
        case 12u: return 64u;
        case 13u: return 128u;
        case 14u: return 256u;
        default: return 512u;
    }
}

static uint32_t apb_divider_from_bits(uint32_t bits) {
    if ((bits & 0x4u) == 0u) return 1u;
    return UINT32_C(1) << ((bits & 0x3u) + 1u);
}

static das_result_t pll1_source_frequency(uint32_t hse_frequency_hz,
                                          uint32_t* frequency_hz) {
    if (frequency_hz == 0) return DAS_ERROR_INVALID_ARGUMENT;

    switch (RCC->PLLCKSELR & RCC_PLLCKSELR_PLLSRC) {
        case RCC_PLLCKSELR_PLLSRC_HSI:
            *frequency_hz = hsi_frequency_hz();
            return DAS_OK;
        case RCC_PLLCKSELR_PLLSRC_CSI:
            *frequency_hz = STM32H755_CSI_HZ;
            return DAS_OK;
        case RCC_PLLCKSELR_PLLSRC_HSE:
            if (hse_frequency_hz == 0u) return DAS_ERROR_UNSUPPORTED;
            *frequency_hz = hse_frequency_hz;
            return DAS_OK;
        default:
            return DAS_ERROR_UNSUPPORTED;
    }
}

static das_result_t system_frequency_hz(uint32_t hse_frequency_hz,
                                        uint32_t* frequency_hz) {
    if (frequency_hz == 0) return DAS_ERROR_INVALID_ARGUMENT;

    switch (RCC->CFGR & RCC_CFGR_SWS) {
        case RCC_CFGR_SWS_HSI:
            *frequency_hz = hsi_frequency_hz();
            return DAS_OK;
        case RCC_CFGR_SWS_CSI:
            *frequency_hz = STM32H755_CSI_HZ;
            return DAS_OK;
        case RCC_CFGR_SWS_HSE:
            if (hse_frequency_hz == 0u) return DAS_ERROR_UNSUPPORTED;
            *frequency_hz = hse_frequency_hz;
            return DAS_OK;
        case RCC_CFGR_SWS_PLL1: {
            uint32_t source_hz = 0u;
            const das_result_t source_result =
                pll1_source_frequency(hse_frequency_hz, &source_hz);
            if (source_result != DAS_OK) return source_result;

            const uint32_t m =
                (RCC->PLLCKSELR & RCC_PLLCKSELR_DIVM1) >> RCC_PLLCKSELR_DIVM1_Pos;
            const uint32_t n =
                ((RCC->PLL1DIVR & RCC_PLL1DIVR_N1) >> RCC_PLL1DIVR_N1_Pos) + 1u;
            const uint32_t p =
                ((RCC->PLL1DIVR & RCC_PLL1DIVR_P1) >> RCC_PLL1DIVR_P1_Pos) + 1u;
            if (m == 0u || p == 0u) return DAS_ERROR_UNSUPPORTED;

            uint64_t multiplier_8192 = (uint64_t)n * UINT64_C(8192);
            if ((RCC->PLLCFGR & RCC_PLLCFGR_PLL1FRACEN) != 0u) {
                multiplier_8192 +=
                    (RCC->PLL1FRACR & RCC_PLL1FRACR_FRACN1) >>
                    RCC_PLL1FRACR_FRACN1_Pos;
            }

            const uint64_t numerator = (uint64_t)source_hz * multiplier_8192;
            const uint64_t denominator =
                (uint64_t)m * UINT64_C(8192) * (uint64_t)p;
            *frequency_hz = (uint32_t)(numerator / denominator);
            return DAS_OK;
        }
        default:
            return DAS_ERROR_UNSUPPORTED;
    }
}

das_result_t stm32h755_clock_get_frequencies(
    uint32_t hse_frequency_hz,
    stm32h755_clock_frequencies_t* frequencies) {
    if (frequencies == 0) return DAS_ERROR_INVALID_ARGUMENT;

    uint32_t system_hz = 0u;
    const das_result_t result = system_frequency_hz(hse_frequency_hz, &system_hz);
    if (result != DAS_OK) return result;

    const uint32_t d1_div = core_ahb_divider_from_bits(
        (RCC->D1CFGR & RCC_D1CFGR_D1CPRE) >> RCC_D1CFGR_D1CPRE_Pos);
    const uint32_t cm7_hz = system_hz / d1_div;
    const uint32_t ahb_div = core_ahb_divider_from_bits(
        (RCC->D1CFGR & RCC_D1CFGR_HPRE) >> RCC_D1CFGR_HPRE_Pos);
    const uint32_t ahb_hz = cm7_hz / ahb_div;

    frequencies->system_hz = system_hz;
    frequencies->cm7_hz = cm7_hz;
    frequencies->cm4_hz = ahb_hz;
    frequencies->ahb_hz = ahb_hz;
    frequencies->apb1_hz = ahb_hz / apb_divider_from_bits(
        (RCC->D2CFGR & RCC_D2CFGR_D2PPRE1) >> RCC_D2CFGR_D2PPRE1_Pos);
    frequencies->apb2_hz = ahb_hz / apb_divider_from_bits(
        (RCC->D2CFGR & RCC_D2CFGR_D2PPRE2) >> RCC_D2CFGR_D2PPRE2_Pos);
    frequencies->apb3_hz = ahb_hz / apb_divider_from_bits(
        (RCC->D1CFGR & RCC_D1CFGR_D1PPRE) >> RCC_D1CFGR_D1PPRE_Pos);
    frequencies->apb4_hz = ahb_hz / apb_divider_from_bits(
        (RCC->D3CFGR & RCC_D3CFGR_D3PPRE) >> RCC_D3CFGR_D3PPRE_Pos);
    return DAS_OK;
}

#if defined(CORE_CM7)

static das_result_t wait_mask(volatile uint32_t* reg,
                              uint32_t mask,
                              bool set,
                              uint32_t wait_limit) {
    for (uint32_t poll = 0u; poll < wait_limit; ++poll) {
        const bool observed = (*reg & mask) != 0u;
        if (observed == set) return DAS_OK;
    }
    return DAS_ERROR_TIMEOUT;
}

static bool apb_divider_bits(uint8_t divider, uint32_t* bits) {
    if (bits == 0) return false;
    switch (divider) {
        case 1u: *bits = 0u; return true;
        case 2u: *bits = 4u; return true;
        case 4u: *bits = 5u; return true;
        case 8u: *bits = 6u; return true;
        case 16u: *bits = 7u; return true;
        default: return false;
    }
}

static bool core_ahb_divider_bits(uint16_t divider, uint32_t* bits) {
    if (bits == 0) return false;
    switch (divider) {
        case 1u: *bits = 0u; return true;
        case 2u: *bits = 8u; return true;
        case 4u: *bits = 9u; return true;
        case 8u: *bits = 10u; return true;
        case 16u: *bits = 11u; return true;
        case 64u: *bits = 12u; return true;
        case 128u: *bits = 13u; return true;
        case 256u: *bits = 14u; return true;
        case 512u: *bits = 15u; return true;
        default: return false;
    }
}

static das_result_t derive_frequencies(const stm32h755_clock_config_t* config,
                                       stm32h755_clock_frequencies_t* frequencies) {
    if (config == 0 || frequencies == 0) return DAS_ERROR_INVALID_ARGUMENT;

    uint32_t source_hz = 0u;
    switch (config->source) {
        case STM32H755_CLOCK_SOURCE_HSI:
            source_hz = STM32H755_HSI_HZ;
            break;
        case STM32H755_CLOCK_SOURCE_HSE:
        case STM32H755_CLOCK_SOURCE_HSE_BYPASS:
            if (config->source_hz < STM32H755_HSE_MIN_HZ ||
                config->source_hz > STM32H755_HSE_MAX_HZ) {
                return DAS_ERROR_INVALID_ARGUMENT;
            }
            source_hz = config->source_hz;
            break;
        default:
            return DAS_ERROR_INVALID_ARGUMENT;
    }

    uint64_t system_hz = source_hz;
    if (config->use_pll1) {
        if (config->pll_m == 0u || config->pll_m > 63u ||
            config->pll_n < 4u || config->pll_n > 512u ||
            config->pll_p == 0u || config->pll_p > 128u ||
            config->pll_q == 0u || config->pll_q > 128u ||
            config->pll_r == 0u || config->pll_r > 128u ||
            source_hz % config->pll_m != 0u) {
            return DAS_ERROR_INVALID_ARGUMENT;
        }

        const uint32_t reference_hz = source_hz / config->pll_m;
        if (reference_hz < UINT32_C(1000000) ||
            reference_hz > UINT32_C(16000000)) {
            return DAS_ERROR_INVALID_ARGUMENT;
        }

        const uint64_t vco_hz = (uint64_t)reference_hz * config->pll_n;
        if (vco_hz < STM32H755_PLL_WIDE_VCO_MIN_HZ ||
            vco_hz > STM32H755_PLL_WIDE_VCO_MAX_HZ) {
            return DAS_ERROR_INVALID_ARGUMENT;
        }
        system_hz = vco_hz / config->pll_p;
    }

    uint32_t ignored_bits = 0u;
    if (!core_ahb_divider_bits(config->d1_core_divider, &ignored_bits) ||
        !core_ahb_divider_bits(config->ahb_divider, &ignored_bits) ||
        !apb_divider_bits(config->apb1_divider, &ignored_bits) ||
        !apb_divider_bits(config->apb2_divider, &ignored_bits) ||
        !apb_divider_bits(config->apb3_divider, &ignored_bits) ||
        !apb_divider_bits(config->apb4_divider, &ignored_bits) ||
        system_hz > UINT32_MAX) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    frequencies->system_hz = (uint32_t)system_hz;
    frequencies->cm7_hz = frequencies->system_hz / config->d1_core_divider;
    frequencies->ahb_hz = frequencies->cm7_hz / config->ahb_divider;
    frequencies->cm4_hz = frequencies->ahb_hz;
    frequencies->apb1_hz = frequencies->ahb_hz / config->apb1_divider;
    frequencies->apb2_hz = frequencies->ahb_hz / config->apb2_divider;
    frequencies->apb3_hz = frequencies->ahb_hz / config->apb3_divider;
    frequencies->apb4_hz = frequencies->ahb_hz / config->apb4_divider;
    return DAS_OK;
}

static das_result_t validate_managed_config(
    const stm32h755_clock_config_t* config,
    stm32h755_clock_frequencies_t* frequencies) {
    if (config == 0 || frequencies == 0 || config->wait_limit == 0u) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    const das_result_t result = derive_frequencies(config, frequencies);
    if (result != DAS_OK) return result;

    if (frequencies->cm7_hz > STM32H755_MANAGED_CM7_MAX_HZ ||
        frequencies->ahb_hz > STM32H755_MANAGED_AHB_MAX_HZ ||
        frequencies->apb1_hz > STM32H755_MANAGED_APB_MAX_HZ ||
        frequencies->apb2_hz > STM32H755_MANAGED_APB_MAX_HZ ||
        frequencies->apb3_hz > STM32H755_MANAGED_APB_MAX_HZ ||
        frequencies->apb4_hz > STM32H755_MANAGED_APB_MAX_HZ) {
        return DAS_ERROR_UNSUPPORTED;
    }
    return DAS_OK;
}

static uint32_t pll_reference_range_bits(uint32_t reference_hz) {
    if (reference_hz < UINT32_C(2000000)) return RCC_PLLCFGR_PLL1RGE_0;
    if (reference_hz < UINT32_C(4000000)) return RCC_PLLCFGR_PLL1RGE_1;
    if (reference_hz < UINT32_C(8000000)) return RCC_PLLCFGR_PLL1RGE_2;
    return RCC_PLLCFGR_PLL1RGE_3;
}

static das_result_t switch_to_hsi(uint32_t wait_limit) {
    RCC->CR = (RCC->CR & ~RCC_CR_HSIDIV) | RCC_CR_HSION | RCC_CR_HSIDIV_1;
    das_result_t result = wait_mask(&RCC->CR, RCC_CR_HSIRDY, true, wait_limit);
    if (result != DAS_OK) return result;

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
    for (uint32_t poll = 0u; poll < wait_limit; ++poll) {
        if ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_HSI) return DAS_OK;
    }
    return DAS_ERROR_TIMEOUT;
}

static das_result_t configure_source(const stm32h755_clock_config_t* config) {
    if (config->source == STM32H755_CLOCK_SOURCE_HSI) {
        RCC->CR = (RCC->CR & ~RCC_CR_HSIDIV) | RCC_CR_HSION | RCC_CR_HSIDIV_1;
        return wait_mask(&RCC->CR, RCC_CR_HSIRDY, true, config->wait_limit);
    }

    RCC->CR &= ~RCC_CR_HSEON;
    das_result_t result =
        wait_mask(&RCC->CR, RCC_CR_HSERDY, false, config->wait_limit);
    if (result != DAS_OK) return result;

    if (config->source == STM32H755_CLOCK_SOURCE_HSE_BYPASS) {
        RCC->CR |= RCC_CR_HSEBYP;
    } else {
        RCC->CR &= ~RCC_CR_HSEBYP;
    }
    RCC->CR |= RCC_CR_HSEON;
    return wait_mask(&RCC->CR, RCC_CR_HSERDY, true, config->wait_limit);
}

static das_result_t configure_pll1(const stm32h755_clock_config_t* config) {
    RCC->CR &= ~RCC_CR_PLL1ON;
    das_result_t result =
        wait_mask(&RCC->CR, RCC_CR_PLL1RDY, false, config->wait_limit);
    if (result != DAS_OK || !config->use_pll1) return result;

    const uint32_t source_bits = config->source == STM32H755_CLOCK_SOURCE_HSI
        ? RCC_PLLCKSELR_PLLSRC_HSI
        : RCC_PLLCKSELR_PLLSRC_HSE;
    const uint32_t source_hz = config->source == STM32H755_CLOCK_SOURCE_HSI
        ? STM32H755_HSI_HZ
        : config->source_hz;
    const uint32_t reference_hz = source_hz / config->pll_m;

    RCC->PLLCKSELR =
        (RCC->PLLCKSELR & ~(RCC_PLLCKSELR_PLLSRC | RCC_PLLCKSELR_DIVM1)) |
        source_bits |
        ((uint32_t)config->pll_m << RCC_PLLCKSELR_DIVM1_Pos);

    RCC->PLL1DIVR =
        ((uint32_t)(config->pll_n - 1u) << RCC_PLL1DIVR_N1_Pos) |
        ((uint32_t)(config->pll_p - 1u) << RCC_PLL1DIVR_P1_Pos) |
        ((uint32_t)(config->pll_q - 1u) << RCC_PLL1DIVR_Q1_Pos) |
        ((uint32_t)(config->pll_r - 1u) << RCC_PLL1DIVR_R1_Pos);
    RCC->PLL1FRACR = 0u;

    RCC->PLLCFGR =
        (RCC->PLLCFGR & ~(RCC_PLLCFGR_PLL1FRACEN |
                          RCC_PLLCFGR_PLL1VCOSEL |
                          RCC_PLLCFGR_PLL1RGE |
                          RCC_PLLCFGR_DIVP1EN |
                          RCC_PLLCFGR_DIVQ1EN |
                          RCC_PLLCFGR_DIVR1EN)) |
        pll_reference_range_bits(reference_hz) |
        RCC_PLLCFGR_DIVP1EN |
        RCC_PLLCFGR_DIVQ1EN |
        RCC_PLLCFGR_DIVR1EN;

    RCC->CR |= RCC_CR_PLL1ON;
    return wait_mask(&RCC->CR, RCC_CR_PLL1RDY, true, config->wait_limit);
}

static void configure_bus_dividers(const stm32h755_clock_config_t* config) {
    uint32_t d1_bits = 0u, ahb_bits = 0u;
    uint32_t apb1_bits = 0u, apb2_bits = 0u, apb3_bits = 0u, apb4_bits = 0u;
    (void)core_ahb_divider_bits(config->d1_core_divider, &d1_bits);
    (void)core_ahb_divider_bits(config->ahb_divider, &ahb_bits);
    (void)apb_divider_bits(config->apb1_divider, &apb1_bits);
    (void)apb_divider_bits(config->apb2_divider, &apb2_bits);
    (void)apb_divider_bits(config->apb3_divider, &apb3_bits);
    (void)apb_divider_bits(config->apb4_divider, &apb4_bits);

    RCC->D1CFGR =
        (RCC->D1CFGR & ~(RCC_D1CFGR_D1CPRE | RCC_D1CFGR_HPRE | RCC_D1CFGR_D1PPRE)) |
        (d1_bits << RCC_D1CFGR_D1CPRE_Pos) |
        (ahb_bits << RCC_D1CFGR_HPRE_Pos) |
        (apb3_bits << RCC_D1CFGR_D1PPRE_Pos);
    RCC->D2CFGR =
        (RCC->D2CFGR & ~(RCC_D2CFGR_D2PPRE1 | RCC_D2CFGR_D2PPRE2)) |
        (apb1_bits << RCC_D2CFGR_D2PPRE1_Pos) |
        (apb2_bits << RCC_D2CFGR_D2PPRE2_Pos);
    RCC->D3CFGR =
        (RCC->D3CFGR & ~RCC_D3CFGR_D3PPRE) |
        (apb4_bits << RCC_D3CFGR_D3PPRE_Pos);
}

static das_result_t switch_to_target(const stm32h755_clock_config_t* config) {
    uint32_t sw_bits = 0u;
    uint32_t sws_bits = 0u;
    if (config->use_pll1) {
        sw_bits = RCC_CFGR_SW_PLL1;
        sws_bits = RCC_CFGR_SWS_PLL1;
    } else if (config->source == STM32H755_CLOCK_SOURCE_HSI) {
        sw_bits = RCC_CFGR_SW_HSI;
        sws_bits = RCC_CFGR_SWS_HSI;
    } else {
        sw_bits = RCC_CFGR_SW_HSE;
        sws_bits = RCC_CFGR_SWS_HSE;
    }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | sw_bits;
    for (uint32_t poll = 0u; poll < config->wait_limit; ++poll) {
        if ((RCC->CFGR & RCC_CFGR_SWS) == sws_bits) return DAS_OK;
    }
    return DAS_ERROR_TIMEOUT;
}

#endif /* CORE_CM7 */

das_result_t stm32h755_clock_apply(const stm32h755_clock_config_t* config) {
#if defined(CORE_CM4)
    (void)config;
    return DAS_ERROR_UNSUPPORTED;
#elif defined(CORE_CM7)
    stm32h755_clock_frequencies_t expected = {0};
    const das_result_t validation = validate_managed_config(config, &expected);
    if (validation != DAS_OK) return validation;

    das_result_t result = switch_to_hsi(config->wait_limit);
    if (result != DAS_OK) return result;

    PWR->D3CR = (PWR->D3CR & ~PWR_D3CR_VOS) | STM32H755_MANAGED_VOS;
    result = wait_mask(&PWR->D3CR, PWR_D3CR_VOSRDY, true, config->wait_limit);
    if (result != DAS_OK) return result;

    FLASH->ACR =
        (FLASH->ACR & ~(FLASH_ACR_LATENCY | FLASH_ACR_WRHIGHFREQ)) |
        STM32H755_MANAGED_FLASH_LATENCY |
        STM32H755_MANAGED_FLASH_PROGRAM_DELAY;
    if ((FLASH->ACR & (FLASH_ACR_LATENCY | FLASH_ACR_WRHIGHFREQ)) !=
        (STM32H755_MANAGED_FLASH_LATENCY | STM32H755_MANAGED_FLASH_PROGRAM_DELAY)) {
        return DAS_ERROR_UNSUPPORTED;
    }

    /* Apply conservative divisors before increasing SYSCLK. */
    configure_bus_dividers(config);

    result = configure_source(config);
    if (result != DAS_OK) return result;
    result = configure_pll1(config);
    if (result != DAS_OK) return result;
    result = switch_to_target(config);
    if (result != DAS_OK) return result;

    __DSB();
    __ISB();

    stm32h755_clock_frequencies_t observed = {0};
    result = stm32h755_clock_get_frequencies(config->source_hz, &observed);
    if (result != DAS_OK) return result;
    if (observed.system_hz != expected.system_hz ||
        observed.cm7_hz != expected.cm7_hz ||
        observed.cm4_hz != expected.cm4_hz ||
        observed.ahb_hz != expected.ahb_hz ||
        observed.apb1_hz != expected.apb1_hz ||
        observed.apb2_hz != expected.apb2_hz ||
        observed.apb3_hz != expected.apb3_hz ||
        observed.apb4_hz != expected.apb4_hz) {
        return DAS_ERROR_UNSUPPORTED;
    }
    return DAS_OK;
#else
#error "STM32H755 clock backend requires CORE_CM7 or CORE_CM4"
#endif
}
