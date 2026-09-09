# Architecture

DAS is a compile-time device abstraction stack. Its purpose is to keep application code stable while separating concerns that vendor HALs often collapse together: portable API, CPU/core architecture, silicon device peripherals, and board wiring.

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

Hardware-independent shared behavior belongs here when multiple implementations would otherwise duplicate the same policy or algorithm.

### MCU/core architecture layer

Location:

```text
src/mcu/<architecture>/
```

Current target:

```text
src/mcu/cortex_m/
```

This layer contains **CPU/core architecture support only**, in C and/or assembly.

Valid examples:

- reset/runtime initialization;
- core exception defaults;
- NVIC access;
- SCB helpers;
- SysTick;
- PRIMASK/BASEPRI helpers;
- Cortex-M cache/MPU primitives.

Invalid examples:

- STM32 GPIO;
- STM32 RCC/PWR/FLASH;
- STM32 EXTI/SYSCFG routing;
- USART/UART;
- DMA/DMAMUX;
- STM32 timers, SPI, I2C, ADC.

Those are device features, not Cortex-M features.

#### Current Cortex-M startup support

`src/mcu/cortex_m/startup.c` provides a reusable optional reset/runtime path.

The weak `Reset_Handler`:

1. copies `.data` from `__data_load__` to the RAM range `__data_start__`..`__data_end__`;
2. clears `__bss_start__`..`__bss_end__`;
3. programs the architecturally defined SCB VTOR register from `__vector_table_start__`;
4. executes DSB/ISB barriers;
5. calls `main()`;
6. remains in a non-returning idle loop if `main()` returns.

Weak default handlers are supplied for Cortex-M core exceptions. Because they are weak, a bootloader, RTOS, or application can replace them with strong definitions.

The core layer deliberately does **not** define vendor external-IRQ vector layouts. A concrete device/application image still owns its external interrupt vector table.

### Device layer

Location:

```text
src/device/<device>/
```

Current target:

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

Current target:

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

The intended conceptual direction is:

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

Current implementation sources:

```text
src/mcu/cortex_m/startup.c
src/device/stm32h755/gpio.c
src/board/nucleo_h755zi_q/board.c
```

There is no runtime target discovery.

## CMSIS boundary

For STM32H755, DAS consumes:

```text
CMSIS-Core: core_cm7.h
CMSIS device: stm32h755xx.h
```

CMSIS-Core is appropriate for Cortex-M architectural definitions. The STM32H755 device header is appropriate inside the device/backend and target test code that needs silicon register definitions.

DAS does not compile or link STM32 HAL or LL source code.

## Startup, vector-table and linker ownership

The reusable Cortex-M startup is part of the library, but it remains **optional and overridable** because its reset/core handlers are weak.

The final firmware image still owns:

- the vector table itself;
- device-specific external IRQ entries;
- the linker script and physical memory map;
- the symbols consumed by the reusable reset path;
- system clock configuration;
- application/RTOS/bootloader policy.

The reusable startup expects these linker symbols:

```text
__data_load__
__data_start__
__data_end__
__bss_start__
__bss_end__
__vector_table_start__
```

The STM32H755 qualification image currently provides those symbols through its test-local linker script. Reusable STM32H755 linker/memory-layout support is tracked separately.

This separation allows the same Cortex-M startup mechanics to coexist with custom bootloaders, RTOS startup code, alternative memory maps, or custom vector placement.

## Interrupt ownership

There are two distinct concerns:

- Cortex-M NVIC/core interrupt control belongs in `src/mcu/cortex_m/`;
- STM32H755 interrupt-source routing, such as EXTI/SYSCFG, belongs in `src/device/stm32h755/`.

The current GPIO API configures the STM32 EXTI line and pending state. The hardware-test application still owns direct NVIC calls until the dedicated Cortex-M NVIC API is implemented.

## Qualification rule

A backend is not considered complete merely because it compiles.

Where practical, qualification should combine:

- configuration evidence: registers contain the intended values;
- execution evidence: the API path actually executes;
- physical evidence: a signal traverses a real pin/wire/peripheral path.

The current campaign additionally qualifies Cortex-M startup by corrupting `.data` and `.bss`, resetting the target, and verifying runtime restoration plus VTOR placement before continuing with GPIO and LED tests.
