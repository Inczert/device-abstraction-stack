// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_I2C_H
#define DAS_I2C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <das/result.h>

/** Opaque I2C-controller handle. storage is backend-owned. */
typedef struct das_i2c {
    uint32_t storage;
} das_i2c_t;

#define DAS_I2C_INVALID ((das_i2c_t){UINT32_MAX})
#define DAS_I2C_WAIT_FOREVER UINT32_MAX

/**
 * I2C controller configuration.
 *
 * frequency_hz is the requested bus rate. The STM32H755 baseline supports
 * Standard-mode 100 kHz and Fast-mode 400 kHz and derives TIMINGR from the
 * live peripheral kernel clock rather than from a generated Cube constant.
 */
typedef struct das_i2c_config {
    uint32_t frequency_hz;
} das_i2c_config_t;

/** Return true when the active backend recognizes the controller handle. */
bool das_i2c_is_valid(das_i2c_t i2c);

/** Configure an already-resolved I2C controller. */
das_result_t das_i2c_init(das_i2c_t i2c, const das_i2c_config_t* config);

/** Return the nominal bus frequency represented by the live timing registers. */
das_result_t das_i2c_get_frequency(das_i2c_t i2c, uint32_t* frequency_hz);

/**
 * Address-only probe using a write-direction START followed by STOP.
 *
 * A NACK is returned as DAS_ERROR_IO. The baseline supports 7-bit addresses.
 */
das_result_t das_i2c_probe_timeout(das_i2c_t i2c,
                                   uint8_t address,
                                   uint32_t timeout_ms);
das_result_t das_i2c_probe(das_i2c_t i2c, uint8_t address);

/** Write bytes to a 7-bit addressed target. */
das_result_t das_i2c_write_timeout(das_i2c_t i2c,
                                   uint8_t address,
                                   const uint8_t* data,
                                   size_t size,
                                   uint32_t timeout_ms);
das_result_t das_i2c_write(das_i2c_t i2c,
                           uint8_t address,
                           const uint8_t* data,
                           size_t size);

/** Read bytes from a 7-bit addressed target. */
das_result_t das_i2c_read_timeout(das_i2c_t i2c,
                                  uint8_t address,
                                  uint8_t* data,
                                  size_t size,
                                  uint32_t timeout_ms);
das_result_t das_i2c_read(das_i2c_t i2c,
                          uint8_t address,
                          uint8_t* data,
                          size_t size);

/**
 * Combined write-then-read transaction using a repeated START.
 *
 * Either phase may be zero length, in which case this reduces to a normal
 * read or write operation. The baseline bounds each non-empty phase to 255
 * bytes; longer transfers can be added with RELOAD support later.
 */
das_result_t das_i2c_write_read_timeout(das_i2c_t i2c,
                                        uint8_t address,
                                        const uint8_t* write_data,
                                        size_t write_size,
                                        uint8_t* read_data,
                                        size_t read_size,
                                        uint32_t timeout_ms);
das_result_t das_i2c_write_read(das_i2c_t i2c,
                                uint8_t address,
                                const uint8_t* write_data,
                                size_t write_size,
                                uint8_t* read_data,
                                size_t read_size);

#endif
