// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_CACHE_H
#define DAS_CACHE_H

#include <stdbool.h>
#include <stddef.h>

#include <das/result.h>

/** Return true when the selected CPU has a data cache. */
bool das_cache_data_available(void);

/** Return true when the data cache is currently enabled. */
bool das_cache_data_is_enabled(void);

/** Return the data-cache line size in bytes, or zero when no D-cache exists. */
size_t das_cache_data_line_size(void);

/** Enable the CPU data cache when the selected core provides one. */
das_result_t das_cache_data_enable(void);

/** Clean and disable the CPU data cache when the selected core provides one. */
das_result_t das_cache_data_disable(void);

/**
 * Clean every cache line touched by [address, address + size).
 *
 * The implementation expands the range to complete cache lines. On a core
 * without a data cache this is a successful no-op.
 */
das_result_t das_cache_data_clean(const void* address, size_t size);

/**
 * Invalidate every cache line touched by [address, address + size).
 *
 * The implementation expands the range to complete cache lines. Callers must
 * avoid invalidating dirty unrelated data that shares an edge cache line.
 * DMA destination buffers should therefore be cache-line isolated or cleaned
 * before invalidation. On a core without a data cache this is a successful
 * no-op.
 */
das_result_t das_cache_data_invalidate(void* address, size_t size);

/** Clean and invalidate every cache line touched by the supplied range. */
das_result_t das_cache_data_clean_invalidate(void* address, size_t size);

#endif
