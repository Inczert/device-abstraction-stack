# Cortex-M core backend

This directory is the Cortex-M CPU/core implementation layer.

It intentionally contains no STM32 peripheral code. Device-specific interrupt routing, clocks, GPIO, serial peripherals and DMA belong under `src/device/<device>/`.

## Current support

`startup.c` provides an optional reusable Cortex-M reset/runtime path:

- copies `.data` from its load address into RAM;
- clears `.bss`;
- programs the architecturally defined SCB VTOR register from the linker-provided `__vector_table_start__` symbol;
- executes the required DSB/ISB barriers;
- calls the application's `main()`;
- provides weak default handlers for Cortex-M core exceptions.

`Reset_Handler` and the default exception handlers are weak. Applications with a bootloader, RTOS startup, custom runtime initialization, or their own exception policy can replace them with strong definitions.

The Cortex-M layer does **not** define STM32 external interrupt vectors. A target image must still provide a vector table with the device-specific external IRQ layout it actually uses.

## Linker contract

The reusable startup path expects the final firmware linker script to export:

```text
__data_load__
__data_start__
__data_end__
__bss_start__
__bss_end__
__vector_table_start__
```

The STM32H755 qualification linker script currently provides that contract. Reusable STM32H755 linker/memory-layout support is tracked separately in issue #3.

## Planned core work

Upcoming Cortex-M-only work includes NVIC helpers, SysTick/timebase support, and Cortex-M7 cache/MPU helpers.
