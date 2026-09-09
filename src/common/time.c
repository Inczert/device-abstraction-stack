// SPDX-License-Identifier: Apache-2.0

#include <das/time.h>

#include <stddef.h>

static das_time_source_fn_t g_time_source;
static void* g_time_source_context;

static bool interval_valid(uint32_t interval_ms) {
    return interval_ms <= DAS_TIME_MAX_INTERVAL_MS;
}

das_result_t das_time_set_source(das_time_source_fn_t source, void* context) {
    if (source == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    g_time_source_context = context;
    g_time_source = source;
    return DAS_OK;
}

bool das_time_is_ready(void) {
    return g_time_source != 0;
}

das_time_ms_t das_time_now_ms(void) {
    das_time_source_fn_t source = g_time_source;
    if (source == 0) {
        return 0u;
    }
    return source(g_time_source_context);
}

uint32_t das_time_elapsed_ms(das_time_ms_t start_ms) {
    if (!das_time_is_ready()) {
        return 0u;
    }
    return (uint32_t)(das_time_now_ms() - start_ms);
}

bool das_time_interval_elapsed(das_time_ms_t start_ms, uint32_t interval_ms) {
    return das_time_is_ready() &&
           interval_valid(interval_ms) &&
           das_time_elapsed_ms(start_ms) >= interval_ms;
}

das_result_t das_time_deadline_after(uint32_t delay_ms,
                                     das_time_ms_t* deadline_ms) {
    if (deadline_ms == 0 || !interval_valid(delay_ms)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (!das_time_is_ready()) {
        return DAS_ERROR_NOT_READY;
    }

    *deadline_ms = das_time_now_ms() + delay_ms;
    return DAS_OK;
}

bool das_time_deadline_reached(das_time_ms_t deadline_ms) {
    if (!das_time_is_ready()) {
        return false;
    }

    const uint32_t delta = (uint32_t)(das_time_now_ms() - deadline_ms);
    return delta <= DAS_TIME_MAX_INTERVAL_MS;
}

das_result_t das_delay_ms(uint32_t duration_ms) {
    if (!interval_valid(duration_ms)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    if (duration_ms == 0u) {
        return DAS_OK;
    }
    if (!das_time_is_ready()) {
        return DAS_ERROR_NOT_READY;
    }

    const das_time_ms_t start_ms = das_time_now_ms();
    while ((uint32_t)(das_time_now_ms() - start_ms) < duration_ms) {
        /* Busy wait: the selected source must advance independently. */
    }
    return DAS_OK;
}
