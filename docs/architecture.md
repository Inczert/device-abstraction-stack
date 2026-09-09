# Architecture

DAS is a compile-time device abstraction stack. It separates portable APIs, CPU/core architecture, silicon-specific device support, board wiring, and build-time memory policy instead of collapsing them into one vendor framework.

## Layer boundaries

### Public API

```text
include/das/
```

Application-facing contracts. Public headers use DAS and standard C types only; vendor/CMSIS device types do not leak here.

### Common logic

```text
src/common/
```

Hardware-independent behavior shared by implementations.

### MCU/core architecture layer

```text
src/mcu/<architecture>/
```

Current target:

```text
src/mcu/cortex_m/
```

This layer contains CPU/core architecture support only, whether implemented in C or assembly. Current support includes reset/runtime initialization and weak core exception handlers. NVIC, SysTick and Cortex-M7 cache/MPU helpers belong here as they are added.

STM32 GPIO, RCC, USART, DMA and EXTI/SYSCFG do not belong here because they are not Cortex-M features.

### Device layer

```text
src/device/<device>/
```

Current target:

```text
src/device/stm32h755/
```

This layer contains STM32H755 on-chip peripheral/register behavior. The current implementation contains GPIO and EXTI/SYSCFG routing. Future RCC/PWR/FLASH, USART, DMA/DMAMUX, timers, SPI, I2C, ADC and watchdog support belongs here.

### Board layer

```text
src/board/<board>/
```

Current target:

```text
src/board/nucleo_h755zi_q/
```

This layer maps board-level resources to device capabilities. Current mappings are the three NUCLEO user LEDs. Future resources include the user button, connector buses, ST-LINK VCOM and physical clock-source wiring.

### Device/build memory policy

Reusable physical memory layout is device-specific but is not C source code, so DAS keeps it under:

```text
cmake/targets/
```

Current default:

```text
cmake/targets/stm32h755_cm7.ld
```

The linker script models STM32H755 memory and exports the symbols expected by the reusable Cortex-M reset path. It is surfaced through the optional `das::linker` CMake interface target.

This separation is deliberate:

```text
Cortex-M startup mechanics       -> src/mcu/cortex_m/
STM32H755 memory addresses       -> cmake/targets/stm32h755_cm7.ld
STM32H755 peripherals            -> src/device/stm32h755/
NUCLEO physical wiring           -> src/board/nucleo_h755zi_q/
```

## Dependency direction

```text
application / RTOS
        |
        v
public DAS API
        |
   +----+-------------------+
   |                        |
   v                        v
common                   board mapping
                            |
                            v
                         device
                            |
                            v
                        MCU/core
                            |
                            v
                          CMSIS
                            |
                            v
                         hardware

final firmware link
        |
        +--> optional das::linker --> selected device memory layout
```

Important rules:

1. `include/das/` exposes no STM32/CMSIS device types.
2. `src/mcu/` contains no vendor peripheral register programming.
3. `src/device/` contains no board connector/LED/button assumptions.
4. `src/board/` does not duplicate device register programming.
5. Common code depends on neither a specific device nor board.
6. A default linker script is optional policy, not a hidden requirement of `das::das`.

## Current target composition

```text
DAS_DEVICE=nucleo_h755zi_q

MCU/core backend = cortex_m
Device backend   = stm32h755
Board backend    = nucleo_h755zi_q
Linker default   = cmake/targets/stm32h755_cm7.ld
```

The code library and linker policy are separate targets:

```text
das::das       reusable C implementation
das::linker    optional selected -T linker script
```

There is no runtime target discovery.

## CMSIS boundary

For STM32H755 DAS consumes CMSIS-Core plus `stm32h755xx.h`. STM32 device headers stay inside device/target implementation code. DAS does not compile or link STM32 HAL or LL sources.

## Startup, vector table and linker ownership

The reusable Cortex-M `Reset_Handler` is weak and optional. It expects linker symbols for initialized data, zero-initialized data and vector placement.

The default STM32H755 script now supplies that contract:

```text
__data_load__
__data_start__
__data_end__
__bss_start__
__bss_end__
__vector_table_start__
```

The final image still owns the actual vector-table entries, including all device-specific external IRQ vectors.

Applications with a bootloader, RTOS, alternate flash origin, bank split, shared-memory scheme or special TCM placement can override `DAS_LINKER_SCRIPT` or omit `das::linker` and supply their own script.

See [STM32H755 Cortex-M7 memory layout](memory-layout.md).

## Interrupt ownership

Cortex-M NVIC/core interrupt control belongs in `src/mcu/cortex_m/`. STM32H755 interrupt-source routing, such as EXTI/SYSCFG, belongs in `src/device/stm32h755/`.

The current hardware-test image still owns direct NVIC calls until the dedicated Cortex-M NVIC API is implemented.

## Qualification rule

A backend/build layer is not complete merely because it compiles.

The current campaign qualifies:

- ELF/linker-map placement before hardware execution;
- Cortex-M runtime reconstruction across reset;
- register configuration and execution state;
- physical GPIO/EXTI signal paths;
- visible board LED behavior.

The memory-layout check verifies the generated map/ELF before OpenOCD is even started, while the startup test then proves those linker symbols work on the actual MCU after reset.
