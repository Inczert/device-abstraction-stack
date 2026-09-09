# Cortex-M core backend

This directory contains **Cortex-M architecture code only**. It deliberately contains no STM32 peripheral implementation.

The same architecture layer is shared by the STM32H755 Cortex-M7 and Cortex-M4 builds.

## Current support

`startup.c` provides an optional reusable reset/runtime path:

- copy `.data` from its linker-defined load address into RAM;
- clear `.bss`;
- set SCB VTOR from `__vector_table_start__`;
- execute DSB/ISB barriers;
- call `main()`;
- remain in a non-returning loop if `main()` returns;
- provide weak default handlers for Cortex-M core exceptions.

The reset/core handlers are weak so an RTOS, bootloader or application can provide strong replacements.

## What this layer does not know

The Cortex-M startup code does not know:

- STM32H755 flash addresses;
- which flash bank a core image uses;
- AXI/D2 SRAM placement;
- STM32 external IRQ numbers;
- Nucleo board wiring.

Those belong to device/build/board layers.

## Linker contract

If the DAS reset path is used, the final linker script must export:

```text
__data_load__
__data_start__
__data_end__
__bss_start__
__bss_end__
__vector_table_start__
```

The default STM32H755 scripts provide that contract for both selected cores:

```text
DAS_CORE=cm7 -> cmake/targets/stm32h755_cm7.ld
DAS_CORE=cm4 -> cmake/targets/stm32h755_cm4.ld
```

A custom `DAS_LINKER_SCRIPT` may provide different physical addresses while retaining the same startup contract.

## Core selection

`DAS_CORE` selects compiler/core details outside this source file:

```text
cm7 -> Cortex-M7 + FPv5-D16 + CORE_CM7
cm4 -> Cortex-M4 + FPv4-SP-D16 + CORE_CM4
```

This keeps one reusable Cortex-M startup implementation while the device backend and build system select the appropriate CPU-specific views and memory policy.

## Planned architecture work

Upcoming core-only features include:

- NVIC helpers;
- SysTick/timebase;
- interrupt masking helpers;
- Cortex-M7 cache/MPU support where architecture-specific.

STM32 RCC, GPIO, EXTI, USART, DMA and other peripherals belong under `src/device/stm32h755/`, not here.
