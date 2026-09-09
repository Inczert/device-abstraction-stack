# Architecture

DAS is a compile-time device abstraction stack. Its purpose is to keep application code stable while separating four concerns that are often collapsed into one vendor HAL: portable API, CPU/core architecture, silicon device peripherals, and board wiring.

## Layer boundaries

### Public API

Location:

```text
include/das/
```

This is the application contract. Public headers use DAS and standard C types only. Vendor/CMSIS device types must not leak into this layer.

### Common logic

Location:

```text
src/common/
```

Hardware-independent shared behavior belongs here when multiple backends would otherwise duplicate the same policy or algorithm.

### MCU/core architecture layer

Location:

```text
src/mcu/<architecture>/
```

For the current target:

```text
src/mcu/cortex_m/
```

This layer contains **CPU/core architecture support only**, whether implemented in C or assembly.

Valid examples:

- exception/core helpers;
- NVIC access;
- SCB helpers;
- SysTick;
- PRIMASK/BASEPRI helpers;
- Cortex-M cache/MPU primitives;
- reusable Cortex-M startup/vector primitives.

Invalid examples:

- STM32 GPIO;
- STM32 RCC/PWR/FLASH;
- EXTI/SYSCFG routing;
- USART/UART;
- DMA/DMAMUX;
- STM32 timers, SPI, I2C, ADC.

Those are device features, not Cortex-M features.

The core layer may use CMSIS-Core definitions where useful, but must not acquire a dependency on one STM32 device.

### Device layer

Location:

```text
src/device/<device>/
```

For the current target:

```text
src/device/stm32h755/
```

This layer implements on-chip peripherals and silicon-specific control using the selected device's register definitions.

Current implementation:

```text
src/device/stm32h755/gpio.c
```

Future examples include RCC/PWR/FLASH, EXTI/SYSCFG, USART, DMA/DMAMUX, timers, SPI, I2C, ADC, watchdog, and dual-core device control.

Device code may include the STM32H755 CMSIS device header internally. Those types must not appear in the public API.

### Board layer

Location:

```text
src/board/<board>/
```

For the current target:

```text
src/board/nucleo_h755zi_q/
```

This layer describes physical wiring and named board resources. It should consume public/device capabilities rather than duplicate register programming.

Current mappings:

```text
DAS_BOARD_LED_GREEN   -> PB0  / LD1
DAS_BOARD_LED_YELLOW  -> PE1  / LD2
DAS_BOARD_LED_RED     -> PB14 / LD3
```

Future board resources may include the user button, ST-LINK virtual COM mapping, connector buses, fixed transceiver enables, and physical clock-source information.

Do not turn the board layer into a one-for-one alias table for every MCU pin.

## Dependency direction

The intended direction is:

```text
application
    |
    v
public DAS API
    |
    +----------------------+
    |                      |
    v                      v
common logic           board mapping
                           |
                           v
                       device layer
                           |
                           v
                      MCU/core layer
                           |
                           v
                        CMSIS
                           |
                           v
                       hardware
```

The conceptual dependency is from board semantics toward device capabilities and from device support toward reusable core primitives. In practice, public peripheral APIs are implemented by the selected device source files, while board helpers call those public APIs.

Important rules:

1. `include/das/` exposes no STM32/CMSIS device types.
2. `src/mcu/` contains no vendor peripheral register programming.
3. `src/device/` contains no board connector/LED/button assumptions.
4. `src/board/` does not duplicate device register programming.
5. Application code does not include implementation files from `src/`.
6. Common code depends on neither a specific device nor a specific board.

## Current build composition

The public selector remains:

```text
DAS_DEVICE=nucleo_h755zi_q
```

For that target CMake composes:

```text
MCU/core backend = cortex_m
Device backend   = stm32h755
Board backend    = nucleo_h755zi_q
```

The name of the selector is retained for compatibility while the internal composition is explicit.

There is no runtime target discovery. Embedded firmware knows its hardware before link time, so DAS pays no runtime abstraction cost to rediscover it.

## CMSIS boundary

For STM32H755, DAS currently consumes:

```text
CMSIS-Core: core_cm7.h
CMSIS device: stm32h755xx.h
```

CMSIS-Core is appropriate for Cortex-M architectural definitions. The STM32H755 device header is appropriate only inside the device/backend and test code that needs silicon register definitions.

DAS does not compile or link STM32 HAL or LL source code.

## Startup and linker ownership

The static library currently does not provide the consuming application's reset handler or linker script.

The hardware qualification image under:

```text
tests/hardware/stm32h755/
```

contains a minimal test-specific startup and linker script because it must boot independently.

Reusable Cortex-M startup belongs in the MCU/core layer. STM32H755 memory-map/linker support belongs to the device/build layer. Applications must remain able to override both for bootloaders, RTOSes, custom memory partitions, or special vector placement.

## Interrupt ownership

There are two distinct concerns:

- Cortex-M NVIC/core interrupt control belongs in `src/mcu/cortex_m/`;
- STM32H755 interrupt-source routing, such as EXTI/SYSCFG, belongs in `src/device/stm32h755/`.

The current GPIO API configures the STM32 EXTI line and pending state. The test firmware currently owns the NVIC call directly; the planned Cortex-M NVIC layer will replace that architecture leakage without moving EXTI into the core layer.

## Qualification rule

A backend is not considered complete merely because it compiles.

Where practical, qualification should combine:

- configuration evidence: registers contain the intended values;
- execution evidence: the API path actually executes;
- physical evidence: a signal traverses a real pin/wire/peripheral path.

The existing STM32H755 GPIO campaign uses physical loopback for input, open-drain, and EXTI, plus visual LED confirmation. Future peripheral drivers should follow the same standard.
