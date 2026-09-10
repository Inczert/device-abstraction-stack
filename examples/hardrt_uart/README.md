# HardRT + DAS UART example

This example demonstrates one ownership model for using [HardRT](https://github.com/ExoSpaceLabs/hardrt) as the Cortex-M scheduler while DAS owns board clocking and peripherals.

Both dependencies are consumed as installed CMake packages:

```cmake
find_package(DAS CONFIG REQUIRED)
find_package(HardRT CONFIG REQUIRED)
target_link_libraries(das_hardrt_uart PRIVATE HardRT::hardrt das::das)
```

## Ownership

- DAS owns STM32H755/NUCLEO clock setup and UART/GPIO access.
- HardRT owns `SysTick_Handler`, `PendSV_Handler`, the scheduler tick and context switching.
- DAS supplies the reset/runtime path, default vector table and linker layout.
- Strong HardRT core handlers replace the weak DAS defaults in that vector table.
- `das_time_init()` is deliberately **not** called. DAS time is redirected to `hrt_now_ms()` with `das_time_set_source()`.

The application sets the DAS 400 MHz board profile **before** initializing either UART or HardRT, queries the live CM7 core rate, and passes that exact value as `hrt_config_t.core_hz`. Changing the core clock after HardRT has configured SysTick would invalidate the RTOS tick period, and changing it after UART initialization would invalidate UART baud programming, so runtime clock changes require coordinated reconfiguration rather than an casual `das_clock_set_frequency()` call.

HardRT currently expects generic `__RAM_START__`/`__RAM_END__` linker symbols for task-stack validation. DAS exports those aliases from its default core-specific writable RAM region so the RTOS can use the DAS linker policy without vendor startup/linker files.

The example has one LED task and one UART task. Only the UART task owns the console; applications with multiple UART users should serialize access with an RTOS mutex or central I/O task because the current DAS UART API does not provide internal task-level locking.

CI cross-links this example against current HardRT `main` and verifies that the final ELF contains strong HardRT `PendSV_Handler` and `SysTick_Handler` symbols plus the DAS vector table.
