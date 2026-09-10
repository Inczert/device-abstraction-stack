// SPDX-License-Identifier: Apache-2.0

#include <das/dma.h>
#include <das/spi.h>
#include <das/time.h>

#include "dma_internal.h"
#include "spi_internal.h"
#include "stm32h755xx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DAS_STM32H755_SPI_MAX_TRANSFER UINT32_C(0xffff)
#define DAS_STM32H755_SPI_ERROR_FLAGS (SPI_SR_OVR | SPI_SR_MODF)
#define DAS_STM32H755_SPI_CLEAR_FLAGS \
    (SPI_IFCR_EOTC | SPI_IFCR_TXTFC | SPI_IFCR_OVRC | SPI_IFCR_MODFC)

#if defined(CORE_CM7)
#define DAS_RCC_CORE RCC_C1
#elif defined(CORE_CM4)
#define DAS_RCC_CORE RCC_C2
#else
#error "STM32H755 SPI backend requires CORE_CM7 or CORE_CM4"
#endif

typedef struct spi_wait {
    bool finite;
    das_time_ms_t start_ms;
    uint32_t timeout_ms;
} spi_wait_t;

static bool config_valid(const das_spi_config_t* config) {
    if (config == 0 || config->frequency_hz == 0u) {
        return false;
    }
    if ((unsigned)config->mode > (unsigned)DAS_SPI_MODE_3) {
        return false;
    }
    return config->bit_order == DAS_SPI_MSB_FIRST ||
           config->bit_order == DAS_SPI_LSB_FIRST;
}

static SPI_TypeDef* resolve_spi(das_spi_t spi) {
    return spi.storage == STM32H755_SPI1 ? SPI1 : 0;
}

static uint32_t hsi_frequency_hz(void) {
    switch (RCC->CR & RCC_CR_HSIDIV) {
        case RCC_CR_HSIDIV_2: return UINT32_C(32000000);
        case RCC_CR_HSIDIV_4: return UINT32_C(16000000);
        case RCC_CR_HSIDIV_8: return UINT32_C(8000000);
        default: return UINT32_C(64000000);
    }
}

static uint32_t spi123_perck_bits(void) {
    return UINT32_C(4) << RCC_D2CCIP1R_SPI123SEL_Pos;
}

static das_result_t kernel_frequency(uint32_t* frequency_hz) {
    if (frequency_hz == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if ((RCC->CR & RCC_CR_HSIRDY) == 0u ||
        (RCC->D1CCIPR & RCC_D1CCIPR_CKPERSEL) != 0u ||
        (RCC->D2CCIP1R & RCC_D2CCIP1R_SPI123SEL) != spi123_perck_bits()) {
        return DAS_ERROR_NOT_READY;
    }

    *frequency_hz = hsi_frequency_hz();
    return *frequency_hz == 0u ? DAS_ERROR_NOT_READY : DAS_OK;
}

static das_result_t select_kernel_clock(uint32_t* frequency_hz) {
    if (frequency_hz == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if ((RCC->CR & RCC_CR_HSIRDY) == 0u) {
        return DAS_ERROR_NOT_READY;
    }

    /* SPI1/2/3 select PER_CK; PER_CK selects the live HSI oscillator. */
    RCC->D1CCIPR &= ~RCC_D1CCIPR_CKPERSEL;
    RCC->D2CCIP1R =
        (RCC->D2CCIP1R & ~RCC_D2CCIP1R_SPI123SEL) |
        spi123_perck_bits();
    __DSB();

    return kernel_frequency(frequency_hz);
}

static void enable_peripheral_clock(void) {
    DAS_RCC_CORE->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)DAS_RCC_CORE->APB2ENR;
    __DSB();
}

static das_result_t select_prescaler(uint32_t kernel_hz,
                                     uint32_t requested_hz,
                                     uint32_t* mbr,
                                     uint32_t* effective_hz) {
    if (kernel_hz == 0u || requested_hz == 0u || mbr == 0 || effective_hz == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    for (uint32_t exponent = 0u; exponent <= 7u; ++exponent) {
        const uint32_t divider = UINT32_C(2) << exponent;
        const uint32_t rate = kernel_hz / divider;
        if (rate <= requested_hz) {
            *mbr = exponent;
            *effective_hz = rate;
            return rate == 0u ? DAS_ERROR_UNSUPPORTED : DAS_OK;
        }
    }
    return DAS_ERROR_UNSUPPORTED;
}

static das_result_t prepare_wait(uint32_t timeout_ms, spi_wait_t* wait) {
    if (wait == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (timeout_ms == DAS_SPI_WAIT_FOREVER) {
        *wait = (spi_wait_t){.finite = false, .start_ms = 0u, .timeout_ms = 0u};
        return DAS_OK;
    }
    if (timeout_ms > DAS_TIME_MAX_INTERVAL_MS) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (!das_time_is_ready()) {
        return DAS_ERROR_NOT_READY;
    }

    *wait = (spi_wait_t){
        .finite = true,
        .start_ms = das_time_now_ms(),
        .timeout_ms = timeout_ms,
    };
    return DAS_OK;
}

static bool wait_expired(const spi_wait_t* wait) {
    return wait->finite &&
           das_time_interval_elapsed(wait->start_ms, wait->timeout_ms);
}

static void abort_transfer(SPI_TypeDef* registers) {
    registers->CR1 &= ~SPI_CR1_SPE;
    registers->IFCR = DAS_STM32H755_SPI_CLEAR_FLAGS;
}

static das_result_t transfer_chunk(SPI_TypeDef* registers,
                                   const uint8_t* tx,
                                   uint8_t* rx,
                                   size_t offset,
                                   uint32_t count,
                                   const spi_wait_t* wait) {
    registers->CR1 &= ~SPI_CR1_SPE;
    registers->IFCR = DAS_STM32H755_SPI_CLEAR_FLAGS;
    registers->CR2 = count & SPI_CR2_TSIZE;

    registers->CR1 |= SPI_CR1_SSI | SPI_CR1_SPE;
    registers->CR1 |= SPI_CR1_CSTART;

    uint32_t tx_index = 0u;
    uint32_t rx_index = 0u;

    while (tx_index < count || rx_index < count) {
        const uint32_t status = registers->SR;
        if ((status & DAS_STM32H755_SPI_ERROR_FLAGS) != 0u) {
            abort_transfer(registers);
            return DAS_ERROR_IO;
        }

        if (tx_index < count && (status & SPI_SR_TXP) != 0u) {
            const uint8_t value = tx != 0 ? tx[offset + tx_index] : UINT8_C(0xff);
            *(volatile uint8_t*)&registers->TXDR = value;
            ++tx_index;
        }

        if (rx_index < count && (registers->SR & SPI_SR_RXP) != 0u) {
            const uint8_t value = *(volatile uint8_t*)&registers->RXDR;
            if (rx != 0) {
                rx[offset + rx_index] = value;
            }
            ++rx_index;
        }

        if (wait_expired(wait)) {
            abort_transfer(registers);
            return DAS_ERROR_TIMEOUT;
        }
    }

    while ((registers->SR & SPI_SR_EOT) == 0u) {
        if ((registers->SR & DAS_STM32H755_SPI_ERROR_FLAGS) != 0u) {
            abort_transfer(registers);
            return DAS_ERROR_IO;
        }
        if (wait_expired(wait)) {
            abort_transfer(registers);
            return DAS_ERROR_TIMEOUT;
        }
    }

    registers->CR1 &= ~SPI_CR1_SPE;
    registers->IFCR = DAS_STM32H755_SPI_CLEAR_FLAGS;
    return DAS_OK;
}

static void dma_pair_cleanup(SPI_TypeDef* registers,
                             das_dma_t rx_dma,
                             das_dma_t tx_dma) {
    registers->CR1 &= ~SPI_CR1_SPE;
    registers->CFG1 &= ~(SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);
    registers->IFCR = DAS_STM32H755_SPI_CLEAR_FLAGS;
    if (das_dma_is_valid(rx_dma)) {
        (void)das_dma_abort(rx_dma);
        (void)das_dma_release(rx_dma);
    }
    if (das_dma_is_valid(tx_dma)) {
        (void)das_dma_abort(tx_dma);
        (void)das_dma_release(tx_dma);
    }
}

static das_result_t transfer_dma_chunk(SPI_TypeDef* registers,
                                       const uint8_t* tx,
                                       uint8_t* rx,
                                       size_t offset,
                                       uint32_t count,
                                       const spi_wait_t* wait) {
    das_dma_t rx_dma = DAS_DMA_INVALID;
    das_dma_t tx_dma = DAS_DMA_INVALID;
    das_result_t result = das_dma_acquire(&rx_dma);
    if (result != DAS_OK) return result;
    result = das_dma_acquire(&tx_dma);
    if (result != DAS_OK) {
        (void)das_dma_release(rx_dma);
        return result;
    }

    const das_dma_config_t rx_config = {
        .direction = DAS_DMA_PERIPHERAL_TO_MEMORY,
        .source_width = DAS_DMA_WIDTH_BYTE,
        .destination_width = DAS_DMA_WIDTH_BYTE,
        .source_increment = false,
        .destination_increment = true,
    };
    const das_dma_config_t tx_config = {
        .direction = DAS_DMA_MEMORY_TO_PERIPHERAL,
        .source_width = DAS_DMA_WIDTH_BYTE,
        .destination_width = DAS_DMA_WIDTH_BYTE,
        .source_increment = true,
        .destination_increment = false,
    };

    result = das_dma_configure(rx_dma, &rx_config);
    if (result == DAS_OK) result = das_dma_configure(tx_dma, &tx_config);
    if (result == DAS_OK) {
        result = stm32h755_dma_set_request(rx_dma, STM32H755_DMA_REQUEST_SPI1_RX);
    }
    if (result == DAS_OK) {
        result = stm32h755_dma_set_request(tx_dma, STM32H755_DMA_REQUEST_SPI1_TX);
    }
    if (result != DAS_OK) {
        dma_pair_cleanup(registers, rx_dma, tx_dma);
        return result;
    }

    registers->CR1 &= ~SPI_CR1_SPE;
    registers->IFCR = DAS_STM32H755_SPI_CLEAR_FLAGS;
    registers->CR2 = count & SPI_CR2_TSIZE;
    registers->CFG1 |= SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN;

    result = das_dma_start(rx_dma,
                           (const void*)&registers->RXDR,
                           &rx[offset],
                           count);
    if (result == DAS_OK) {
        result = das_dma_start(tx_dma,
                               &tx[offset],
                               (void*)&registers->TXDR,
                               count);
    }
    if (result != DAS_OK) {
        dma_pair_cleanup(registers, rx_dma, tx_dma);
        return result;
    }

    registers->CR1 |= SPI_CR1_SSI | SPI_CR1_SPE;
    registers->CR1 |= SPI_CR1_CSTART;

    for (;;) {
        das_dma_state_t rx_state = DAS_DMA_STATE_IDLE;
        das_dma_state_t tx_state = DAS_DMA_STATE_IDLE;
        result = das_dma_get_state(rx_dma, &rx_state);
        if (result == DAS_OK) result = das_dma_get_state(tx_dma, &tx_state);
        if (result != DAS_OK) {
            dma_pair_cleanup(registers, rx_dma, tx_dma);
            return result;
        }

        const uint32_t status = registers->SR;
        if (rx_state == DAS_DMA_STATE_ERROR || tx_state == DAS_DMA_STATE_ERROR ||
            (status & DAS_STM32H755_SPI_ERROR_FLAGS) != 0u) {
            dma_pair_cleanup(registers, rx_dma, tx_dma);
            return DAS_ERROR_IO;
        }
        if (rx_state == DAS_DMA_STATE_COMPLETE &&
            tx_state == DAS_DMA_STATE_COMPLETE &&
            (status & SPI_SR_EOT) != 0u) {
            break;
        }
        if (wait_expired(wait)) {
            dma_pair_cleanup(registers, rx_dma, tx_dma);
            return DAS_ERROR_TIMEOUT;
        }
    }

    dma_pair_cleanup(registers, rx_dma, tx_dma);
    return DAS_OK;
}

das_spi_t stm32h755_spi_handle(stm32h755_spi_instance_t instance) {
    return instance == STM32H755_SPI1
        ? (das_spi_t){.storage = (uint32_t)instance}
        : DAS_SPI_INVALID;
}

bool das_spi_is_valid(das_spi_t spi) {
    return resolve_spi(spi) != 0;
}

das_result_t das_spi_init(das_spi_t spi, const das_spi_config_t* config) {
    SPI_TypeDef* const registers = resolve_spi(spi);
    if (registers == 0 || !config_valid(config)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    enable_peripheral_clock();

    uint32_t kernel_hz = 0u;
    das_result_t result = select_kernel_clock(&kernel_hz);
    if (result != DAS_OK) {
        return result;
    }

    uint32_t mbr = 0u;
    uint32_t effective_hz = 0u;
    result = select_prescaler(kernel_hz,
                              config->frequency_hz,
                              &mbr,
                              &effective_hz);
    if (result != DAS_OK) {
        return result;
    }
    (void)effective_hz;

    registers->CR1 &= ~SPI_CR1_SPE;
    registers->IER = 0u;
    registers->CR1 = SPI_CR1_SSI;
    registers->CR2 = 0u;
    registers->CFG1 =
        (UINT32_C(7) << SPI_CFG1_DSIZE_Pos) |
        (mbr << SPI_CFG1_MBR_Pos);

    uint32_t cfg2 = SPI_CFG2_MASTER | SPI_CFG2_SSM | SPI_CFG2_AFCNTR;
    if (config->mode == DAS_SPI_MODE_1 || config->mode == DAS_SPI_MODE_3) {
        cfg2 |= SPI_CFG2_CPHA;
    }
    if (config->mode == DAS_SPI_MODE_2 || config->mode == DAS_SPI_MODE_3) {
        cfg2 |= SPI_CFG2_CPOL;
    }
    if (config->bit_order == DAS_SPI_LSB_FIRST) {
        cfg2 |= SPI_CFG2_LSBFRST;
    }
    registers->CFG2 = cfg2;
    registers->IFCR = DAS_STM32H755_SPI_CLEAR_FLAGS;
    __DSB();
    return DAS_OK;
}

das_result_t das_spi_get_frequency(das_spi_t spi, uint32_t* frequency_hz) {
    SPI_TypeDef* const registers = resolve_spi(spi);
    if (registers == 0 || frequency_hz == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    uint32_t kernel_hz = 0u;
    const das_result_t result = kernel_frequency(&kernel_hz);
    if (result != DAS_OK) {
        return result;
    }

    const uint32_t mbr =
        (registers->CFG1 & SPI_CFG1_MBR) >> SPI_CFG1_MBR_Pos;
    *frequency_hz = kernel_hz / (UINT32_C(2) << mbr);
    return *frequency_hz == 0u ? DAS_ERROR_NOT_READY : DAS_OK;
}

das_result_t das_spi_transfer_timeout(das_spi_t spi,
                                      const uint8_t* tx,
                                      uint8_t* rx,
                                      size_t size,
                                      uint32_t timeout_ms) {
    SPI_TypeDef* const registers = resolve_spi(spi);
    if (registers == 0 || (size != 0u && tx == 0 && rx == 0)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (size == 0u) {
        return DAS_OK;
    }

    spi_wait_t wait;
    das_result_t result = prepare_wait(timeout_ms, &wait);
    if (result != DAS_OK) {
        return result;
    }

    size_t offset = 0u;
    while (offset < size) {
        const size_t remaining = size - offset;
        const uint32_t count = remaining > DAS_STM32H755_SPI_MAX_TRANSFER
            ? DAS_STM32H755_SPI_MAX_TRANSFER
            : (uint32_t)remaining;
        result = transfer_chunk(registers, tx, rx, offset, count, &wait);
        if (result != DAS_OK) {
            return result;
        }
        offset += count;
    }
    return DAS_OK;
}

das_result_t das_spi_transfer(das_spi_t spi,
                              const uint8_t* tx,
                              uint8_t* rx,
                              size_t size) {
    return das_spi_transfer_timeout(spi, tx, rx, size, DAS_SPI_WAIT_FOREVER);
}

das_result_t das_spi_transfer_dma_timeout(das_spi_t spi,
                                          const uint8_t* tx,
                                          uint8_t* rx,
                                          size_t size,
                                          uint32_t timeout_ms) {
    SPI_TypeDef* const registers = resolve_spi(spi);
    if (registers == 0 || (size != 0u && (tx == 0 || rx == 0))) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (size == 0u) return DAS_OK;

    spi_wait_t wait;
    das_result_t result = prepare_wait(timeout_ms, &wait);
    if (result != DAS_OK) return result;

    size_t offset = 0u;
    while (offset < size) {
        const size_t remaining = size - offset;
        const uint32_t count = remaining > DAS_STM32H755_SPI_MAX_TRANSFER
            ? DAS_STM32H755_SPI_MAX_TRANSFER
            : (uint32_t)remaining;
        result = transfer_dma_chunk(registers, tx, rx, offset, count, &wait);
        if (result != DAS_OK) return result;
        offset += count;
    }
    return DAS_OK;
}

das_result_t das_spi_transfer_dma(das_spi_t spi,
                                  const uint8_t* tx,
                                  uint8_t* rx,
                                  size_t size) {
    return das_spi_transfer_dma_timeout(spi,
                                        tx,
                                        rx,
                                        size,
                                        DAS_SPI_WAIT_FOREVER);
}
