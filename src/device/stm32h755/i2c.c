// SPDX-License-Identifier: Apache-2.0

#include <das/i2c.h>
#include <das/time.h>

#include "i2c_internal.h"
#include "stm32h755xx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DAS_STM32H755_I2C_MAX_PHASE UINT32_C(255)
#define DAS_STM32H755_I2C_ERROR_FLAGS \
    (I2C_ISR_NACKF | I2C_ISR_BERR | I2C_ISR_ARLO | I2C_ISR_OVR)
#define DAS_STM32H755_I2C_CLEAR_FLAGS \
    (I2C_ICR_STOPCF | I2C_ICR_NACKCF | I2C_ICR_BERRCF | \
     I2C_ICR_ARLOCF | I2C_ICR_OVRCF)

#if defined(CORE_CM7)
#define DAS_RCC_CORE RCC_C1
#elif defined(CORE_CM4)
#define DAS_RCC_CORE RCC_C2
#else
#error "STM32H755 I2C backend requires CORE_CM7 or CORE_CM4"
#endif

typedef struct i2c_wait {
    bool finite;
    das_time_ms_t start_ms;
    uint32_t timeout_ms;
} i2c_wait_t;

static I2C_TypeDef* resolve_i2c(das_i2c_t i2c) {
    return i2c.storage == STM32H755_I2C1 ? I2C1 : 0;
}

static bool config_valid(const das_i2c_config_t* config) {
    return config != 0 &&
           (config->frequency_hz == UINT32_C(100000) ||
            config->frequency_hz == UINT32_C(400000));
}

static bool address_valid(uint8_t address) {
    return address <= UINT8_C(0x7f);
}

static uint32_t hsi_frequency_hz(void) {
    switch (RCC->CR & RCC_CR_HSIDIV) {
        case RCC_CR_HSIDIV_2: return UINT32_C(32000000);
        case RCC_CR_HSIDIV_4: return UINT32_C(16000000);
        case RCC_CR_HSIDIV_8: return UINT32_C(8000000);
        default: return UINT32_C(64000000);
    }
}

static das_result_t select_kernel_clock(uint32_t* frequency_hz) {
    if (frequency_hz == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if ((RCC->CR & RCC_CR_HSIRDY) == 0u) {
        return DAS_ERROR_NOT_READY;
    }

#if defined(RCC_D2CCIP2R_I2C123SEL)
    RCC->D2CCIP2R =
        (RCC->D2CCIP2R & ~RCC_D2CCIP2R_I2C123SEL) |
        RCC_D2CCIP2R_I2C123SEL_1;
#else
#error "STM32H755 CMSIS header does not expose I2C1/2/3 kernel-clock selection"
#endif
    __DSB();

    *frequency_hz = hsi_frequency_hz();
    return *frequency_hz == 0u ? DAS_ERROR_NOT_READY : DAS_OK;
}

static das_result_t kernel_frequency(uint32_t* frequency_hz) {
    if (frequency_hz == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
#if defined(RCC_D2CCIP2R_I2C123SEL)
    if ((RCC->D2CCIP2R & RCC_D2CCIP2R_I2C123SEL) != RCC_D2CCIP2R_I2C123SEL_1) {
        return DAS_ERROR_NOT_READY;
    }
#else
#error "STM32H755 CMSIS header does not expose I2C1/2/3 kernel-clock selection"
#endif
    if ((RCC->CR & RCC_CR_HSIRDY) == 0u) {
        return DAS_ERROR_NOT_READY;
    }
    *frequency_hz = hsi_frequency_hz();
    return *frequency_hz == 0u ? DAS_ERROR_NOT_READY : DAS_OK;
}

static void enable_peripheral_clock(void) {
    DAS_RCC_CORE->APB1LENR |= RCC_APB1LENR_I2C1EN;
    (void)DAS_RCC_CORE->APB1LENR;
    __DSB();
}

static uint32_t ceil_div_u64(uint64_t numerator, uint64_t denominator) {
    return (uint32_t)((numerator + denominator - UINT64_C(1)) / denominator);
}

static das_result_t calculate_timing(uint32_t kernel_hz,
                                     uint32_t bus_hz,
                                     uint32_t* timing) {
    if (kernel_hz == 0u || timing == 0 ||
        (bus_hz != UINT32_C(100000) && bus_hz != UINT32_C(400000))) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    uint32_t presc_div = 0u;
    uint32_t half_ticks = 0u;
    for (uint32_t candidate = 1u; candidate <= 16u; ++candidate) {
        const uint64_t denominator =
            UINT64_C(2) * (uint64_t)bus_hz * (uint64_t)candidate;
        const uint32_t ticks = ceil_div_u64(kernel_hz, denominator);
        if (ticks >= 2u && ticks <= 256u) {
            presc_div = candidate;
            half_ticks = ticks;
            break;
        }
    }
    if (presc_div == 0u) {
        return DAS_ERROR_UNSUPPORTED;
    }

    const uint32_t scldel_ns = bus_hz <= UINT32_C(100000) ? 500u : 125u;
    const uint32_t sdadel_ns = 100u;
    const uint64_t prescaled_denominator =
        UINT64_C(1000000000) * (uint64_t)presc_div;

    uint32_t scldel_ticks = ceil_div_u64(
        (uint64_t)kernel_hz * scldel_ns,
        prescaled_denominator);
    uint32_t sdadel_ticks = ceil_div_u64(
        (uint64_t)kernel_hz * sdadel_ns,
        prescaled_denominator);

    if (scldel_ticks == 0u) scldel_ticks = 1u;
    if (scldel_ticks > 16u || sdadel_ticks > 15u) {
        return DAS_ERROR_UNSUPPORTED;
    }

    const uint32_t presc = presc_div - 1u;
    const uint32_t scll = half_ticks - 1u;
    const uint32_t sclh = half_ticks - 1u;
    const uint32_t scldel = scldel_ticks - 1u;
    const uint32_t sdadel = sdadel_ticks;

    *timing =
        (presc << I2C_TIMINGR_PRESC_Pos) |
        (scldel << I2C_TIMINGR_SCLDEL_Pos) |
        (sdadel << I2C_TIMINGR_SDADEL_Pos) |
        (sclh << I2C_TIMINGR_SCLH_Pos) |
        (scll << I2C_TIMINGR_SCLL_Pos);
    return DAS_OK;
}

static das_result_t prepare_wait(uint32_t timeout_ms, i2c_wait_t* wait) {
    if (wait == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (timeout_ms == DAS_I2C_WAIT_FOREVER) {
        *wait = (i2c_wait_t){.finite = false, .start_ms = 0u, .timeout_ms = 0u};
        return DAS_OK;
    }
    if (timeout_ms > DAS_TIME_MAX_INTERVAL_MS) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (!das_time_is_ready()) {
        return DAS_ERROR_NOT_READY;
    }
    *wait = (i2c_wait_t){
        .finite = true,
        .start_ms = das_time_now_ms(),
        .timeout_ms = timeout_ms,
    };
    return DAS_OK;
}

static bool wait_expired(const i2c_wait_t* wait) {
    return wait->finite &&
           das_time_interval_elapsed(wait->start_ms, wait->timeout_ms);
}

static void clear_status(I2C_TypeDef* registers) {
    registers->ICR = DAS_STM32H755_I2C_CLEAR_FLAGS;
}

static void abort_transfer(I2C_TypeDef* registers) {
    if ((registers->ISR & I2C_ISR_BUSY) != 0u) {
        registers->CR2 |= I2C_CR2_STOP;
    }
    clear_status(registers);
}

static das_result_t status_error(I2C_TypeDef* registers, uint32_t status) {
    if ((status & I2C_ISR_NACKF) != 0u) {
        registers->ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF;
        return DAS_ERROR_IO;
    }
    if ((status & (I2C_ISR_BERR | I2C_ISR_ARLO | I2C_ISR_OVR)) != 0u) {
        registers->ICR = I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF;
        return DAS_ERROR_IO;
    }
    return DAS_OK;
}

static das_result_t wait_for_set(I2C_TypeDef* registers,
                                 uint32_t flag,
                                 const i2c_wait_t* wait) {
    for (;;) {
        const uint32_t status = registers->ISR;
        const das_result_t error = status_error(registers, status);
        if (error != DAS_OK) {
            return error;
        }
        if ((status & flag) != 0u) {
            return DAS_OK;
        }
        if (wait_expired(wait)) {
            abort_transfer(registers);
            return DAS_ERROR_TIMEOUT;
        }
    }
}

static das_result_t wait_bus_idle(I2C_TypeDef* registers,
                                  const i2c_wait_t* wait) {
    for (;;) {
        if ((registers->ISR & I2C_ISR_BUSY) == 0u) {
            return DAS_OK;
        }
        if (wait_expired(wait)) {
            abort_transfer(registers);
            return DAS_ERROR_TIMEOUT;
        }
    }
}

static uint32_t transfer_cr2(uint8_t address,
                             uint32_t size,
                             bool read,
                             bool autoend) {
    uint32_t cr2 =
        (((uint32_t)address << 1u) & I2C_CR2_SADD) |
        ((size << I2C_CR2_NBYTES_Pos) & I2C_CR2_NBYTES) |
        I2C_CR2_START;
    if (read) cr2 |= I2C_CR2_RD_WRN;
    if (autoend) cr2 |= I2C_CR2_AUTOEND;
    return cr2;
}

static das_result_t finish_autoend(I2C_TypeDef* registers,
                                   const i2c_wait_t* wait) {
    const das_result_t result = wait_for_set(registers, I2C_ISR_STOPF, wait);
    if (result == DAS_OK) {
        registers->ICR = I2C_ICR_STOPCF;
    }
    return result;
}

static das_result_t write_phase(I2C_TypeDef* registers,
                                uint8_t address,
                                const uint8_t* data,
                                uint32_t size,
                                bool autoend,
                                const i2c_wait_t* wait) {
    clear_status(registers);
    registers->CR2 = transfer_cr2(address, size, false, autoend);

    for (uint32_t index = 0u; index < size; ++index) {
        das_result_t result = wait_for_set(registers, I2C_ISR_TXIS, wait);
        if (result != DAS_OK) return result;
        registers->TXDR = data[index];
    }

    return autoend
        ? finish_autoend(registers, wait)
        : wait_for_set(registers, I2C_ISR_TC, wait);
}

static das_result_t read_phase(I2C_TypeDef* registers,
                               uint8_t address,
                               uint8_t* data,
                               uint32_t size,
                               bool autoend,
                               const i2c_wait_t* wait) {
    clear_status(registers);
    registers->CR2 = transfer_cr2(address, size, true, autoend);

    for (uint32_t index = 0u; index < size; ++index) {
        das_result_t result = wait_for_set(registers, I2C_ISR_RXNE, wait);
        if (result != DAS_OK) return result;
        data[index] = (uint8_t)registers->RXDR;
    }

    return autoend
        ? finish_autoend(registers, wait)
        : wait_for_set(registers, I2C_ISR_TC, wait);
}

das_i2c_t stm32h755_i2c_handle(stm32h755_i2c_instance_t instance) {
    return instance == STM32H755_I2C1
        ? (das_i2c_t){.storage = (uint32_t)instance}
        : DAS_I2C_INVALID;
}

bool das_i2c_is_valid(das_i2c_t i2c) {
    return resolve_i2c(i2c) != 0;
}

das_result_t das_i2c_init(das_i2c_t i2c, const das_i2c_config_t* config) {
    I2C_TypeDef* const registers = resolve_i2c(i2c);
    if (registers == 0 || config == 0 || config->frequency_hz == 0u) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (!config_valid(config)) {
        return DAS_ERROR_UNSUPPORTED;
    }

    enable_peripheral_clock();
    uint32_t kernel_hz = 0u;
    das_result_t result = select_kernel_clock(&kernel_hz);
    if (result != DAS_OK) return result;

    uint32_t timing = 0u;
    result = calculate_timing(kernel_hz, config->frequency_hz, &timing);
    if (result != DAS_OK) return result;

    registers->CR1 &= ~I2C_CR1_PE;
    registers->CR1 = 0u;
    registers->CR2 = 0u;
    registers->OAR1 = 0u;
    registers->OAR2 = 0u;
    registers->TIMINGR = timing;
    registers->TIMEOUTR = 0u;
    clear_status(registers);
    registers->CR1 = I2C_CR1_PE;
    __DSB();
    return DAS_OK;
}

das_result_t das_i2c_get_frequency(das_i2c_t i2c, uint32_t* frequency_hz) {
    I2C_TypeDef* const registers = resolve_i2c(i2c);
    if (registers == 0 || frequency_hz == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if ((registers->CR1 & I2C_CR1_PE) == 0u) {
        return DAS_ERROR_NOT_READY;
    }

    uint32_t kernel_hz = 0u;
    const das_result_t result = kernel_frequency(&kernel_hz);
    if (result != DAS_OK) return result;

    const uint32_t timing = registers->TIMINGR;
    const uint32_t presc =
        ((timing & I2C_TIMINGR_PRESC) >> I2C_TIMINGR_PRESC_Pos) + 1u;
    const uint32_t sclh =
        ((timing & I2C_TIMINGR_SCLH) >> I2C_TIMINGR_SCLH_Pos) + 1u;
    const uint32_t scll =
        ((timing & I2C_TIMINGR_SCLL) >> I2C_TIMINGR_SCLL_Pos) + 1u;
    const uint32_t divider = presc * (sclh + scll);
    if (divider == 0u) return DAS_ERROR_NOT_READY;
    *frequency_hz = kernel_hz / divider;
    return *frequency_hz == 0u ? DAS_ERROR_NOT_READY : DAS_OK;
}

das_result_t das_i2c_probe_timeout(das_i2c_t i2c,
                                   uint8_t address,
                                   uint32_t timeout_ms) {
    I2C_TypeDef* const registers = resolve_i2c(i2c);
    if (registers == 0 || !address_valid(address)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if ((registers->CR1 & I2C_CR1_PE) == 0u) return DAS_ERROR_NOT_READY;

    i2c_wait_t wait;
    das_result_t result = prepare_wait(timeout_ms, &wait);
    if (result != DAS_OK) return result;
    result = wait_bus_idle(registers, &wait);
    if (result != DAS_OK) return result;

    clear_status(registers);
    registers->CR2 = transfer_cr2(address, 0u, false, true);
    return finish_autoend(registers, &wait);
}

das_result_t das_i2c_probe(das_i2c_t i2c, uint8_t address) {
    return das_i2c_probe_timeout(i2c, address, DAS_I2C_WAIT_FOREVER);
}

das_result_t das_i2c_write_timeout(das_i2c_t i2c,
                                   uint8_t address,
                                   const uint8_t* data,
                                   size_t size,
                                   uint32_t timeout_ms) {
    I2C_TypeDef* const registers = resolve_i2c(i2c);
    if (registers == 0 || !address_valid(address) ||
        (size != 0u && data == 0)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (size > DAS_STM32H755_I2C_MAX_PHASE) return DAS_ERROR_UNSUPPORTED;
    if ((registers->CR1 & I2C_CR1_PE) == 0u) return DAS_ERROR_NOT_READY;
    if (size == 0u) return das_i2c_probe_timeout(i2c, address, timeout_ms);

    i2c_wait_t wait;
    das_result_t result = prepare_wait(timeout_ms, &wait);
    if (result != DAS_OK) return result;
    result = wait_bus_idle(registers, &wait);
    if (result != DAS_OK) return result;
    return write_phase(registers, address, data, (uint32_t)size, true, &wait);
}

das_result_t das_i2c_write(das_i2c_t i2c,
                           uint8_t address,
                           const uint8_t* data,
                           size_t size) {
    return das_i2c_write_timeout(i2c, address, data, size, DAS_I2C_WAIT_FOREVER);
}

das_result_t das_i2c_read_timeout(das_i2c_t i2c,
                                  uint8_t address,
                                  uint8_t* data,
                                  size_t size,
                                  uint32_t timeout_ms) {
    I2C_TypeDef* const registers = resolve_i2c(i2c);
    if (registers == 0 || !address_valid(address) ||
        (size != 0u && data == 0)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (size == 0u) return DAS_OK;
    if (size > DAS_STM32H755_I2C_MAX_PHASE) return DAS_ERROR_UNSUPPORTED;
    if ((registers->CR1 & I2C_CR1_PE) == 0u) return DAS_ERROR_NOT_READY;

    i2c_wait_t wait;
    das_result_t result = prepare_wait(timeout_ms, &wait);
    if (result != DAS_OK) return result;
    result = wait_bus_idle(registers, &wait);
    if (result != DAS_OK) return result;
    return read_phase(registers, address, data, (uint32_t)size, true, &wait);
}

das_result_t das_i2c_read(das_i2c_t i2c,
                          uint8_t address,
                          uint8_t* data,
                          size_t size) {
    return das_i2c_read_timeout(i2c, address, data, size, DAS_I2C_WAIT_FOREVER);
}

das_result_t das_i2c_write_read_timeout(das_i2c_t i2c,
                                        uint8_t address,
                                        const uint8_t* write_data,
                                        size_t write_size,
                                        uint8_t* read_data,
                                        size_t read_size,
                                        uint32_t timeout_ms) {
    I2C_TypeDef* const registers = resolve_i2c(i2c);
    if (registers == 0 || !address_valid(address) ||
        (write_size != 0u && write_data == 0) ||
        (read_size != 0u && read_data == 0)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (write_size > DAS_STM32H755_I2C_MAX_PHASE ||
        read_size > DAS_STM32H755_I2C_MAX_PHASE) {
        return DAS_ERROR_UNSUPPORTED;
    }
    if (write_size == 0u) {
        return das_i2c_read_timeout(i2c, address, read_data, read_size, timeout_ms);
    }
    if (read_size == 0u) {
        return das_i2c_write_timeout(i2c, address, write_data, write_size, timeout_ms);
    }
    if ((registers->CR1 & I2C_CR1_PE) == 0u) return DAS_ERROR_NOT_READY;

    i2c_wait_t wait;
    das_result_t result = prepare_wait(timeout_ms, &wait);
    if (result != DAS_OK) return result;
    result = wait_bus_idle(registers, &wait);
    if (result != DAS_OK) return result;

    result = write_phase(registers,
                         address,
                         write_data,
                         (uint32_t)write_size,
                         false,
                         &wait);
    if (result != DAS_OK) return result;

    return read_phase(registers,
                      address,
                      read_data,
                      (uint32_t)read_size,
                      true,
                      &wait);
}

das_result_t das_i2c_write_read(das_i2c_t i2c,
                                uint8_t address,
                                const uint8_t* write_data,
                                size_t write_size,
                                uint8_t* read_data,
                                size_t read_size) {
    return das_i2c_write_read_timeout(i2c,
                                      address,
                                      write_data,
                                      write_size,
                                      read_data,
                                      read_size,
                                      DAS_I2C_WAIT_FOREVER);
}
