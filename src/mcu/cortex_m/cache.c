// SPDX-License-Identifier: Apache-2.0

#include <das/cache.h>

#ifndef DAS_CMSIS_DEVICE_HEADER
#error "Cortex-M cache backend requires a CMSIS device header"
#endif

#include DAS_CMSIS_DEVICE_HEADER

#include <limits.h>
#include <stdint.h>

#if defined(CORE_CM7)
#define DAS_CORTEX_M_HAS_DCACHE 1
#define DAS_CORTEX_M_DCACHE_LINE_SIZE 32u
#else
#define DAS_CORTEX_M_HAS_DCACHE 0
#endif

#if DAS_CORTEX_M_HAS_DCACHE
static das_result_t cache_range(const void* address,
                                size_t size,
                                uintptr_t* aligned_address,
                                int32_t* aligned_size) {
    if (size == 0u) {
        *aligned_address = 0u;
        *aligned_size = 0;
        return DAS_OK;
    }
    if (address == 0 || aligned_address == 0 || aligned_size == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    const uintptr_t start = (uintptr_t)address;
    if (size > UINTPTR_MAX - start) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    const uintptr_t end = start + size;
    const uintptr_t mask = (uintptr_t)DAS_CORTEX_M_DCACHE_LINE_SIZE - 1u;
    const uintptr_t first = start & ~mask;
    if (end > UINTPTR_MAX - mask) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    const uintptr_t last = (end + mask) & ~mask;
    const uintptr_t span = last - first;
    if (span > (uintptr_t)INT32_MAX) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    *aligned_address = first;
    *aligned_size = (int32_t)span;
    return DAS_OK;
}
#endif

bool das_cache_data_available(void) {
#if DAS_CORTEX_M_HAS_DCACHE
    return true;
#else
    return false;
#endif
}

bool das_cache_data_is_enabled(void) {
#if DAS_CORTEX_M_HAS_DCACHE
    return (SCB->CCR & SCB_CCR_DC_Msk) != 0u;
#else
    return false;
#endif
}

size_t das_cache_data_line_size(void) {
#if DAS_CORTEX_M_HAS_DCACHE
    return DAS_CORTEX_M_DCACHE_LINE_SIZE;
#else
    return 0u;
#endif
}

das_result_t das_cache_data_enable(void) {
#if DAS_CORTEX_M_HAS_DCACHE
    if (!das_cache_data_is_enabled()) {
        SCB_EnableDCache();
    }
    return DAS_OK;
#else
    return DAS_ERROR_UNSUPPORTED;
#endif
}

das_result_t das_cache_data_disable(void) {
#if DAS_CORTEX_M_HAS_DCACHE
    if (das_cache_data_is_enabled()) {
        SCB_DisableDCache();
    }
    return DAS_OK;
#else
    return DAS_ERROR_UNSUPPORTED;
#endif
}

das_result_t das_cache_data_clean(const void* address, size_t size) {
    if (size == 0u) return DAS_OK;
    if (address == 0) return DAS_ERROR_INVALID_ARGUMENT;
#if DAS_CORTEX_M_HAS_DCACHE
    uintptr_t aligned = 0u;
    int32_t span = 0;
    das_result_t result = cache_range(address, size, &aligned, &span);
    if (result != DAS_OK) return result;
    if (das_cache_data_is_enabled()) {
        SCB_CleanDCache_by_Addr((uint32_t*)aligned, span);
    }
#else
    (void)address;
#endif
    return DAS_OK;
}

das_result_t das_cache_data_invalidate(void* address, size_t size) {
    if (size == 0u) return DAS_OK;
    if (address == 0) return DAS_ERROR_INVALID_ARGUMENT;
#if DAS_CORTEX_M_HAS_DCACHE
    uintptr_t aligned = 0u;
    int32_t span = 0;
    das_result_t result = cache_range(address, size, &aligned, &span);
    if (result != DAS_OK) return result;
    if (das_cache_data_is_enabled()) {
        SCB_InvalidateDCache_by_Addr((uint32_t*)aligned, span);
    }
#else
    (void)address;
#endif
    return DAS_OK;
}

das_result_t das_cache_data_clean_invalidate(void* address, size_t size) {
    if (size == 0u) return DAS_OK;
    if (address == 0) return DAS_ERROR_INVALID_ARGUMENT;
#if DAS_CORTEX_M_HAS_DCACHE
    uintptr_t aligned = 0u;
    int32_t span = 0;
    das_result_t result = cache_range(address, size, &aligned, &span);
    if (result != DAS_OK) return result;
    if (das_cache_data_is_enabled()) {
        SCB_CleanInvalidateDCache_by_Addr((uint32_t*)aligned, span);
    }
#else
    (void)address;
#endif
    return DAS_OK;
}
