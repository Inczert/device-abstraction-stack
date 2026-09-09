# Cortex-M core backend

This directory contains **Cortex-M architecture code only**. It deliberately contains no STM32 peripheral implementation.

The same architecture layer is shared by the STM32H755 Cortex-M7 and Cortex-M4 builds.

## Current support

### Startup/runtime

`startup.c` provides an optional reusable reset/runtime path:

- copy `.data` from its linker-defined load address into RAM;
- clear `.bss`;
- set SCB VTOR from `__vector_table_start__`;
- execute DSB/ISB barriers;
- call `main()`;
- remain in a non-returning loop if `main()` returns;
- provide weak default handlers for Cortex-M core exceptions.

The reset/core handlers are weak so an RTOS, bootloader or application can provide strong replacements.

### Interrupt controller

`irq.c` implements the public device-agnostic `das_irq_*()` contract for Cortex-M by delegating to CMSIS-Core NVIC helpers.

The public handle is `das_irq_t`; applications obtain it from DAS resource APIs such as `das_gpio_interrupt_get_irq()` rather than using `IRQn_Type` or vendor IRQ constants directly.

Supported controller operations are:

- enable/disable/query enabled state;
- set/get priority;
- query the number of implemented priority levels;
- set/clear/query controller pending state.

This implementation intentionally does **not** reproduce the NVIC register layout or CMSIS helper logic. The selected CMSIS device header supplies `IRQn_Type` and `__NVIC_PRIO_BITS`, while CMSIS-Core performs the actual controller accesses.

See [`docs/interrupts.md`](../../../docs/interrupts.md).

## What this layer does not know

The Cortex-M implementation does not own:

- STM32H755 flash addresses;
- which flash bank a core image uses;
- AXI/D2 SRAM placement;
- the meaning of STM32 peripheral interrupt sources;
- GPIO/EXTI routing;
- Nucleo board wiring.

The device layer resolves a peripheral source to the controller-line handle used by the generic IRQ API. For example, the STM32H755 GPIO backend maps a GPIO EXTI source onto the corresponding `das_irq_t`.

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

`DAS_CORE` selects compiler/core details:

```text
cm7 -> Cortex-M7 + FPv5-D16 + CORE_CM7
cm4 -> Cortex-M4 + FPv4-SP-D16 + CORE_CM4
```

For the IRQ backend, the target build also supplies the selected CMSIS device header. This is required because CMSIS-Core deliberately gets the device IRQ enumeration and implemented priority width from the device header. The Cortex-M source itself contains no STM32 IRQ numbers.

## Planned architecture work

Upcoming architecture-level work may include:

- generic monotonic time using CMSIS SysTick internally;
- interrupt masking helpers only where they provide a useful public contract;
- Cortex-M7 cache/MPU support where architecture-specific.

STM32 RCC, GPIO, EXTI, USART, DMA and other peripherals belong under `src/device/stm32h755/`, not here.
