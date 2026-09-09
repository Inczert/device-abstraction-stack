# Architecture

DAS is organized as a compile-time device abstraction stack. Its purpose is to keep application code stable while allowing each supported MCU family and board to have a small, explicit hardware implementation.

The project intentionally avoids a runtime driver registry, dynamic target detection, code generation, or a large vendor HAL dependency.

## Design goals

DAS should provide:

- a small C API that application code can depend on;
- a clear boundary between portable code and MCU-specific register access;
- compile-time selection of the hardware target;
- board-level semantic resources where raw GPIO names are undesirable;
- deterministic behavior suitable for bare-metal and RTOS applications;
- hardware qualification that checks real electrical behavior where possible;
- enough low-level control that using the abstraction does not make the hardware mysterious.

DAS is **not** intended to own:

- reset/startup policy for the consuming application;
- the linker script of the consuming application;
- system-clock policy;
- RTOS or scheduler selection;
- heap policy;
- application interrupt dispatch architecture;
- peripheral middleware protocols.

Those concerns can use DAS, but they remain outside the library unless a future generic API explicitly requires them.

## Layers

### 1. Public API

Location:

```text
include/das/
```

This is the contract visible to applications. Public headers should use generic DAS types and standard C types only.

Application code should not need to include `stm32h755xx.h`, CMSIS register types, or board BSP headers in order to use a DAS peripheral API.

Current public APIs:

- `result.h`
- `gpio.h`
- `board.h`

### 2. Common implementation

Location:

```text
src/common/
```

This layer is reserved for implementation that is independent of a particular MCU family. It should contain shared policy or algorithms when multiple backends would otherwise duplicate identical logic.

The current GPIO implementation is small enough that most behavior still lives directly in the STM32H7 backend.

### 3. MCU-family backend

Location:

```text
src/mcu/<family>/
```

Example:

```text
src/mcu/stm32h7/gpio.c
```

The backend translates generic DAS operations into the registers of one MCU family. This is the layer where CMSIS device definitions are allowed.

For STM32H7, the GPIO backend currently handles:

- GPIO peripheral clock enable;
- `MODER`, `OTYPER`, `OSPEEDR`, `PUPDR` and `AFR` configuration;
- atomic output changes through `BSRR`;
- `ODR` and `IDR` reads;
- SYSCFG EXTI source routing;
- EXTI rising/falling trigger configuration;
- EXTI mask, pending and clear operations.

The backend deliberately uses CMSIS register definitions directly rather than STM32 HAL or LL calls.

### 4. Device / board layer

Location:

```text
src/device/<board>/
```

Example:

```text
src/device/nucleo_h755zi_q/board.c
```

This layer maps semantic board resources to physical peripherals.

For the NUCLEO-H755ZI-Q:

```text
DAS_BOARD_LED_GREEN   -> PB0  / LD1
DAS_BOARD_LED_YELLOW  -> PE1  / LD2
DAS_BOARD_LED_RED     -> PB14 / LD3
```

An application can therefore ask for the green board LED without carrying the Nucleo schematic into its own source code.

A board layer should generally be thin. It is a mapping/policy layer, not another HAL.

### 5. Application

The application links `das::das` and owns the rest of the firmware architecture.

Typical responsibilities include:

- reset handler and startup;
- linker script;
- clock tree;
- application `main`;
- scheduler or RTOS;
- NVIC priority policy;
- interrupt vector functions;
- higher-level device logic.

The application may use the generic API directly or use board-level convenience APIs.

## Dependency direction

Dependencies should flow toward lower layers only:

```text
application
   |
   v
public API
   |
   +--------> board/device mapping
   |                    |
   |                    v
   +--------------> MCU backend
                         |
                         v
                       CMSIS
                         |
                         v
                     hardware
```

Important rules:

1. `include/das/` must not expose STM32 types.
2. Application code should not depend on files under `src/`.
3. MCU backends may include CMSIS device headers.
4. Board code should use public DAS peripheral types rather than duplicating register programming.
5. Common code must not depend on a particular board.
6. Board-specific constants should not leak into generic APIs unless the concept is genuinely portable.

## Compile-time target selection

The current build selects a device with:

```text
DAS_DEVICE=nucleo_h755zi_q
```

There is intentionally no runtime `detect_device()` step. Embedded firmware normally knows its hardware before it is linked, and paying for a runtime abstraction layer would add complexity without solving a useful problem here.

For the current target, CMake selects the STM32H7 implementation and defines the required CMSIS device compilation symbols internally:

```text
CORE_CM7
STM32H755xx
```

As support grows, the target selector should choose only the backend and board sources needed by the requested device.

## CMSIS boundary

CMSIS is used as the register-definition boundary.

For STM32H755, DAS consumes:

```text
Drivers/CMSIS/Core/Include/core_cm7.h
Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h755xx.h
```

Some STM32CubeH7 versions expose the core include directory as `Drivers/CMSIS/Include`; the build accepts either layout.

DAS does not currently compile or link STM32 HAL or LL source code.

## Startup and linker separation

The static `das` library does not provide a reset handler or application linker script.

The hardware qualification image under:

```text
tests/hardware/stm32h755/
```

contains a minimal test-specific startup and linker script because that image must boot independently for qualification. They are not meant to become hidden application dependencies.

This distinction matters: a project should be able to use DAS with its own bare-metal startup, RTOS startup, bootloader arrangement, memory map, or generated system initialization.

## Interrupt ownership

The generic GPIO interrupt API configures the **GPIO-to-EXTI path**. It does not own the application's NVIC or interrupt-vector policy.

For STM32H7, DAS can:

- select the EXTI source port;
- configure rising/falling triggers;
- mask/unmask the EXTI line;
- read and clear pending state.

The consuming firmware still decides:

- which Cortex-M IRQ vector handles the line;
- NVIC priority;
- when to enable the NVIC IRQ;
- what application callback or event system runs from the ISR.

This keeps a GPIO library from quietly becoming an interrupt framework.

## Hardware qualification as part of the architecture

Backends are not considered complete merely because they compile.

Where practical, tests should distinguish:

- **configuration evidence**: registers contain the intended values;
- **execution evidence**: code actually reaches and exercises the API;
- **physical evidence**: a signal leaves one pad and arrives at another, or a visible board resource behaves correctly.

The STM32H755 campaign therefore uses physical GPIO loopback for input, open-drain, and EXTI tests rather than validating only internal register state.

See [Hardware qualification](testing.md).
