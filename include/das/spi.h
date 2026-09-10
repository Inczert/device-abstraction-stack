// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_SPI_H
#define DAS_SPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <das/result.h>

/** Opaque SPI-controller handle. storage is backend-owned. */
typedef struct das_spi {
    uint32_t storage;
} das_spi_t;

#define DAS_SPI_INVALID ((das_spi_t){UINT32_MAX})
#define DAS_SPI_WAIT_FOREVER UINT32_MAX

typedef enum das_spi_mode {
    DAS_SPI_MODE_0 = 0,
    DAS_SPI_MODE_1,
    DAS_SPI_MODE_2,
    DAS_SPI_MODE_3
} das_spi_mode_t;

typedef enum das_spi_bit_order {
    DAS_SPI_MSB_FIRST = 0,
    DAS_SPI_LSB_FIRST
} das_spi_bit_order_t;

/**
 * SPI-controller configuration.
 *
 * frequency_hz is the requested maximum serial clock. Backends choose a
 * supported divider that does not exceed it and expose the effective rate.
 * The baseline transfer unit is eight application bits.
 */
typedef struct das_spi_config {
    uint32_t frequency_hz;
    das_spi_mode_t mode;
    das_spi_bit_order_t bit_order;
} das_spi_config_t;

/** Return true when the active backend recognizes the handle. */
bool das_spi_is_valid(das_spi_t spi);

/** Configure an already-resolved SPI controller. */
das_result_t das_spi_init(das_spi_t spi, const das_spi_config_t* config);

/** Return the effective serial clock in hertz. */
das_result_t das_spi_get_frequency(das_spi_t spi, uint32_t* frequency_hz);

/**
 * Full-duplex polling transfer.
 *
 * tx may be NULL to transmit 0xff fill bytes. rx may be NULL to discard
 * received bytes. They may not both be NULL when size is non-zero.
 * Chip-select is deliberately not asserted by this function.
 */
das_result_t das_spi_transfer_timeout(das_spi_t spi,
                                      const uint8_t* tx,
                                      uint8_t* rx,
                                      size_t size,
                                      uint32_t timeout_ms);

das_result_t das_spi_transfer(das_spi_t spi,
                              const uint8_t* tx,
                              uint8_t* rx,
                              size_t size);

/**
 * Full-duplex DMA transfer using implementation-selected DMA resources.
 *
 * The first DMA baseline requires both tx and rx buffers for non-zero sizes.
 * DMA does not make cacheable memory coherent automatically: on cached CPUs,
 * clean TX data and clean/invalidate RX storage before starting the transfer,
 * then invalidate RX storage after completion. See <das/cache.h>.
 */
das_result_t das_spi_transfer_dma_timeout(das_spi_t spi,
                                          const uint8_t* tx,
                                          uint8_t* rx,
                                          size_t size,
                                          uint32_t timeout_ms);

das_result_t das_spi_transfer_dma(das_spi_t spi,
                                  const uint8_t* tx,
                                  uint8_t* rx,
                                  size_t size);

#endif
