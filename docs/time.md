# Monotonic time

DAS provides a small millisecond monotonic-time API for application scheduling, driver timeouts and simple delays without depending on HAL tick code.

## Public API

```c
#include <das/time.h>

if (das_time_init() != DAS_OK) {
    /* handle initialization failure */
}

das_time_ms_t start = das_time_now_ms();
if (das_time_interval_elapsed(start, 1000u)) {
    /* one second elapsed */
}
```

The default timestamp is a `uint32_t` millisecond counter. It wraps naturally after roughly 49.7 days. DAS elapsed/deadline helpers use modulo arithmetic so callers do not need special wrap handling for relative intervals up to `DAS_TIME_MAX_INTERVAL_MS` (0x7fffffff ms, about 24.8 days).

## Deadline example

```c
das_time_ms_t deadline;
if (das_time_deadline_after(250u, &deadline) == DAS_OK) {
    while (!das_time_deadline_reached(deadline)) {
        /* poll or perform other work */
    }
}
```

`das_time_deadline_after()` rejects longer relative intervals because ordering two wrapping 32-bit timestamps is only unambiguous inside half the counter range.

## Periodic work

A wrap-safe periodic loop can use elapsed time directly:

```c
das_time_ms_t last = das_time_now_ms();

for (;;) {
    if (das_time_interval_elapsed(last, 1000u)) {
        last += 1000u;
        /* periodic work */
    }
}
```

Advancing `last` by the period instead of assigning the current time avoids accumulating the execution time of the periodic work as drift.

## Default Cortex-M backend

For the current NUCLEO-H755ZI-Q target, `das_time_init()`:

1. reads the live frequency of the executing core through `das_clock_get_core_frequency()`;
2. configures Cortex-M SysTick for a 1 kHz interrupt using CMSIS `SysTick_Config()`;
3. installs the SysTick millisecond counter as the active DAS time source.

The public API contains no CMSIS, SysTick, STM32 or `SystemCoreClock` types. CMSIS remains an implementation detail where it is useful.

CM7 and CM4 each own their own SysTick peripheral, so the default timebase is initialized independently on each core.

The default backend should be initialized after selecting the desired board clock profile:

```c
(void)das_clock_set_frequency(400000000u);
(void)das_time_init();
```

If the CPU frequency is changed later, call `das_time_init()` again so the SysTick reload is recalculated. Reconfiguration preserves the millisecond counter itself, so time does not jump backwards merely because the clock profile changed.

## RTOS/application ownership

DAS does not require SysTick ownership. An RTOS or application that already owns the system tick should not call `das_time_init()`. Instead it can install its own millisecond source:

```c
static das_time_ms_t rtos_now(void* context)
{
    (void)context;
    return my_rtos_ticks_in_ms();
}

(void)das_time_set_source(rtos_now, 0);
```

The source callback must progress monotonically modulo `uint32_t` wrap and return milliseconds. Source replacement is intended as initialization/configuration activity; changing the callback concurrently with readers is not currently synchronized.

The reusable Cortex-M startup keeps `SysTick_Handler` weak. When the default DAS time backend is linked, that handler calls an internal DAS SysTick hook. An RTOS/application may replace the weak handler entirely and use an external DAS time source instead.

## Delay semantics

`das_delay_ms()` is intentionally simple. It busy-waits on the active monotonic source and therefore requires that source to advance while the caller is blocked. With the default SysTick backend, interrupts must remain enabled.

This makes it useful for short bring-up/configuration waits, but it is not a scheduler and it does not pretend busy-waiting is a sophisticated RTOS service.

Calling a non-zero delay before a source is installed returns `DAS_ERROR_NOT_READY` rather than hanging forever.

## Hardware qualification

The STM32H755 campaign builds a dedicated time image for both CM7 and CM4. Each image checks:

- no-source delay returns `DAS_ERROR_NOT_READY`;
- application-provided time-source replacement;
- elapsed time across `uint32_t` wrap;
- wrap-safe deadline creation and comparison;
- rejection of ambiguous intervals beyond the half-range limit;
- default SysTick initialization from the live executing-core frequency;
- a 100 ms delay against the Cortex-M DWT cycle counter with a 5% tolerance;
- continued execution after the timing test.

The CM7 time image first selects the qualified 400 MHz board profile, so the SysTick reload is validated against a non-reset clock. CM4 uses the live clock available to CPU2 and independently validates its own SysTick instance.
