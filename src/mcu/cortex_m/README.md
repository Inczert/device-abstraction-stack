# Cortex-M core backend

This directory is the Cortex-M CPU/core implementation layer.

It intentionally contains no STM32 peripheral code. Device-specific interrupt routing, clocks, GPIO, serial peripherals and DMA belong under `src/device/<device>/`.

## Current support

`startup.c` provides an optional reusable Cortex-M reset/runtime path:

- copies `.data` from its load address into RAM;
- clears `.bss`;
- programs the architecturally defined SCB VTOR register from `__vector_table_start__`;
- executes DSB/ISB barriers;
- calls the application's `main()`;
- provides weak default handlers for Cortex-M core exceptions.

`Reset_Handler` and the default exception handlers are weak. Applications with a bootloader, RTOS startup, custom runtime initialization, or their own exception policy can replace them with strong definitions.

The Cortex-M layer does **not** define STM32 external interrupt vectors. A concrete target image owns its device-specific vector-table layout.

## Linker contract

The startup path expects:

```text
__data_load__
__data_start__
__data_end__
__bss_start__
__bss_end__
__vector_table_start__
```

For STM32H755 CM7, DAS now provides a compatible default script at:

```text
cmake/targets/stm32h755_cm7.ld
```

through the optional `das::linker` target. The linker script remains device/build policy rather than Cortex-M source because the physical addresses belong to STM32H755, not to the ARM core architecture.

Applications can override or omit that linker target without replacing the generic Cortex-M startup implementation.

## Planned core work

Upcoming Cortex-M-only work includes NVIC helpers, SysTick/timebase support, and Cortex-M7 cache/MPU helpers.
