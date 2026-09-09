# Porting DAS

DAS grows by composing independent CPU/core, device, and board layers behind stable public APIs. A port should preserve those boundaries rather than introducing a new vendor namespace into application code.

## Porting model

A target is normally composed from:

```text
MCU/core architecture
        +
Device/silicon implementation
        +
Board mapping
        =
DAS target
```

Current target:

```text
Cortex-M
    +
STM32H755
    +
NUCLEO-H755ZI-Q
    =
DAS_DEVICE=nucleo_h755zi_q
```

## 1. Add or reuse the MCU/core layer

Location:

```text
src/mcu/<architecture>/
```

Examples:

```text
src/mcu/cortex_m/
src/mcu/riscv/
```

This layer contains CPU/core architectural support only, in C and/or assembly.

Appropriate responsibilities include:

- exception mechanics;
- interrupt-controller/core helpers;
- core timers such as Cortex-M SysTick;
- cache/MPU primitives;
- architectural barriers/masking;
- reusable core startup primitives.

It must **not** contain vendor peripheral implementations such as STM32 GPIO, RCC, USART, DMA, timers, SPI, I2C or ADC.

A core layer should be reusable across devices that share the same CPU architecture.

## 2. Add the device layer

Location:

```text
src/device/<device>/
```

Example:

```text
src/device/stm32h755/
```

This layer implements silicon-specific peripherals and control using the selected device's register definitions.

For STM32H755, examples include:

- GPIO;
- RCC/PWR/FLASH;
- EXTI/SYSCFG;
- USART/LPUART;
- DMA/DMAMUX;
- timers;
- SPI/I2C;
- ADC/watchdog;
- dual-core device control.

Device code may use CMSIS device headers internally, but vendor types must not leak into public DAS headers.

### Device backend rules

A device implementation should:

- validate public API arguments;
- configure only the hardware it owns;
- avoid unrelated board policy;
- preserve documented public semantics;
- use atomic hardware operations where available;
- avoid heap/RTOS dependencies unless explicitly part of the API;
- return useful errors/timeouts rather than hanging indefinitely;
- return `DAS_ERROR_UNSUPPORTED` when a valid generic operation cannot be represented safely.

## 3. Add the board layer

Location:

```text
src/board/<board>/
```

Example:

```text
src/board/nucleo_h755zi_q/
```

The board layer maps real physical resources onto generic/device capabilities.

Example:

```c
static const das_gpio_pin_t LED_PINS[DAS_BOARD_LED_COUNT] = {
    [DAS_BOARD_LED_GREEN]  = { DAS_GPIO_PORT_B, 0u },
    [DAS_BOARD_LED_YELLOW] = { DAS_GPIO_PORT_E, 1u },
    [DAS_BOARD_LED_RED]    = { DAS_GPIO_PORT_B, 14u },
};
```

Useful board semantics include LEDs, buttons, connector buses, fixed chip-selects/enables, virtual COM mapping, board sensors, and physical clock-source configuration.

Do not use the board layer as a second register backend or as a one-for-one alias table for every MCU pin.

## 4. Extend CMake composition

The selected public target should resolve explicitly to its implementation pieces.

Conceptually:

```cmake
if(DAS_DEVICE STREQUAL "nucleo_h755zi_q")
    set(DAS_MCU_BACKEND cortex_m)
    set(DAS_DEVICE_BACKEND stm32h755)
    set(DAS_BOARD_BACKEND nucleo_h755zi_q)
elseif(...)
    ...
else()
    message(FATAL_ERROR "Unsupported DAS_DEVICE='${DAS_DEVICE}'")
endif()
```

Only sources required for the selected target should be compiled.

As the matrix grows, composition may move into CMake target modules, but the dependency graph must remain explicit.

## 5. Define low-level dependencies

For Cortex-M targets, CMSIS is the preferred boundary where practical:

- CMSIS-Core for CPU/core definitions;
- vendor CMSIS device headers for device registers.

Document:

- required headers;
- how their root path is provided to CMake;
- compiler/device preprocessor symbols;
- whether any vendor source files are linked;
- ABI/toolchain constraints.

Avoid importing an entire SDK when only register definitions are required.

## 6. Keep startup and memory-map responsibilities separated

Reusable CPU startup mechanics belong to the MCU/core layer.

Physical flash/RAM addresses and linker layouts belong to device/build support.

Board clock-source wiring belongs to the board layer.

A hardware qualification image may contain local startup/linker files while reusable support is still being developed, but those test files must not become hidden dependencies of the static library.

Applications must remain free to override startup and linker behavior for bootloaders, RTOSes, custom partitions, or special vector placement.

## 7. Add hardware qualification

A port is not complete because it compiles.

Create target qualification under:

```text
tests/hardware/<target>/
```

with supporting OpenOCD/GDB/build scripts as appropriate.

Qualification should exercise the **public DAS API** and gather independent evidence.

Examples:

- GPIO output: visible board LED and register evidence;
- GPIO input: output-to-input physical jumper;
- pulls: undriven input;
- open-drain: pull-up plus driven/released behavior;
- EXTI: physical edge into an interrupt input;
- UART: TX/RX loopback;
- SPI: MOSI/MISO loopback;
- timers/PWM: counter/capture or external measurement;
- DMA: pattern integrity plus completion/error evidence.

## 8. Package evidence

Hardware campaigns should preserve enough information to diagnose failures after the board is no longer attached.

The current STM32H755 campaign packages:

- build output;
- compiler/tool versions;
- DAS and STM32Cube commit IDs where available;
- OpenOCD log;
- per-case GDB logs;
- ELF;
- linker map;
- symbol table;
- summary and exit status.

## Port acceptance checklist

Before marking a target supported:

- [ ] public API remains vendor-type free;
- [ ] core code contains no vendor peripheral implementation;
- [ ] device code contains no board wiring assumptions;
- [ ] board code does not duplicate device register programming;
- [ ] CMake composes the core/device/board layers explicitly;
- [ ] external dependencies are documented;
- [ ] warnings are treated as errors;
- [ ] target firmware boots independently;
- [ ] configuration/execution evidence passes;
- [ ] physical I/O is exercised where feasible;
- [ ] evidence is archived;
- [ ] documentation/support matrix is updated.

## Adding a new peripheral API

Recommended order:

1. define generic public types and semantics;
2. decide what policy remains application-owned;
3. implement one device backend;
4. add MCU/core helpers only if the feature is genuinely architectural;
5. add board mappings only for real board semantics;
6. build a physical qualification path;
7. update API/integration documentation;
8. then replicate the device backend on other silicon.

This keeps the public API driven by real hardware behavior rather than by vendor brochure feature lists.
