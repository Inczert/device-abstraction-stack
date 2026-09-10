// SPDX-License-Identifier: Apache-2.0

#include <das/time.h>

#include <stdint.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

static das_time_ms_t fake_now(void* context) {
    return *(const uint32_t*)context;
}

static das_time_ms_t stepping_now(void* context) {
    uint32_t* value = (uint32_t*)context;
    return (*value)++;
}

int main(void) {
    uint32_t now = 10u;
    das_time_ms_t deadline = 0u;

    CHECK(!das_time_is_ready());
    CHECK(das_time_now_ms() == 0u);
    CHECK(das_delay_ms(1u) == DAS_ERROR_NOT_READY);
    CHECK(das_time_deadline_after(1u, &deadline) == DAS_ERROR_NOT_READY);
    CHECK(das_time_set_source(0, 0) == DAS_ERROR_INVALID_ARGUMENT);

    CHECK(das_time_set_source(fake_now, &now) == DAS_OK);
    CHECK(das_time_is_ready());
    CHECK(das_time_now_ms() == 10u);
    CHECK(das_time_deadline_after(5u, &deadline) == DAS_OK);
    CHECK(deadline == 15u);
    CHECK(!das_time_deadline_reached(deadline));

    now = 15u;
    CHECK(das_time_deadline_reached(deadline));

    now = UINT32_MAX - 2u;
    const das_time_ms_t start = now;
    now = 3u;
    CHECK(das_time_elapsed_ms(start) == 6u);
    CHECK(das_time_interval_elapsed(start, 6u));
    CHECK(!das_time_interval_elapsed(start, 7u));
    CHECK(!das_time_interval_elapsed(start, UINT32_MAX));
    CHECK(das_time_deadline_after(UINT32_MAX, &deadline) == DAS_ERROR_INVALID_ARGUMENT);

    now = 0u;
    CHECK(das_time_set_source(stepping_now, &now) == DAS_OK);
    CHECK(das_delay_ms(8u) == DAS_OK);
    CHECK(now >= 9u);

    return 0;
}
