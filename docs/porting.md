# Porting DAS to a new MCU or board

DAS is intended to grow by adding small backends and board mappings behind the existing public API. A port should preserve the abstraction boundary rather than teaching application code about the new vendor.

## Porting model

A new target normally consists of two independent pieces:

1. an **MCU-family backend** that implements generic peripheral APIs using that family's registers;
2. a **device/board layer** that maps semantic board resources to real peripherals and pins.

For example, the current target is composed of:

```text
STM32H7 GPIO backend
        +
NUCLEO-H755ZI-Q board mapping
        =
DAS_DEVICE=nucleo_h755zi_q
```

## 1. Add or reuse an MCU-family backend

Create or extend:

```text
src/mcu/<family>/
```

Example:

```text
src/mcu/stm32h7/gpio.c
```

Implement the generic public contract from `include/das/`.

A backend may use vendor-supplied register-definition headers such as CMSIS device headers internally, but those types must not appear in public DAS headers.

### Backend rules

A backend should:

- validate public API arguments;
- enable only the peripheral clocks it requires;
- avoid unrelated global clock-tree or startup policy;
- preserve documented API semantics;
- use atomic hardware operations where the peripheral provides them;
- avoid heap allocation unless a future API explicitly documents it;
- avoid depending on a specific RTOS;
- return `DAS_ERROR_UNSUPPORTED` when a generic operation cannot be represented safely on that backend.

For GPIO specifically, a complete backend should consider:

- input/output/alternate/analog modes;
- pull configuration;
- output type;
- drive/speed configuration;
- alternate-function selection;
- physical input vs output-latch reads;
- interrupt/event routing when supported.

## 2. Add the board/device layer

Create:

```text
src/device/<board>/
```

Example:

```text
src/device/nucleo_h755zi_q/board.c
```

The board layer should map meaningful resources onto the generic peripheral API.

For example:

```c
static const das_gpio_pin_t LED_PINS[DAS_BOARD_LED_COUNT] = {
    [DAS_BOARD_LED_GREEN]  = { DAS_GPIO_PORT_B, 0u },
    [DAS_BOARD_LED_YELLOW] = { DAS_GPIO_PORT_E, 1u },
    [DAS_BOARD_LED_RED]    = { DAS_GPIO_PORT_B, 14u },
};
```

This mapping belongs in the board layer, not scattered through applications.

Do not add a board abstraction merely to rename every MCU pin one-for-one. Use it for resources with genuine board semantics: LEDs, buttons, fixed transceivers, enables, chip selects, board sensors, and similar wiring.

## 3. Extend target selection in CMake

The current top-level CMake accepts:

```text
DAS_DEVICE=nucleo_h755zi_q
```

A new target should extend this selection so that only the required backend/device sources and compile definitions are built.

The desired model is conceptually:

```cmake
if(DAS_DEVICE STREQUAL "nucleo_h755zi_q")
    # STM32H755 / STM32H7 backend + Nucleo mapping
elseif(DAS_DEVICE STREQUAL "some_other_board")
    # required family backend + board mapping
else()
    message(FATAL_ERROR "Unsupported DAS_DEVICE='${DAS_DEVICE}'")
endif()
```

As the target list grows this may be factored into target-specific CMake modules, but the key property should remain explicit compile-time selection.

## 4. Define the external low-level dependency

For Cortex-M devices, CMSIS is the preferred register-definition boundary where practical.

A port should document:

- which low-level headers are required;
- how their path is supplied to CMake;
- which device preprocessor symbols are required;
- whether any vendor source files are linked;
- any compiler/ABI constraints.

Avoid silently importing an entire vendor SDK if the backend only requires a small register-definition subset.

## 5. Keep startup outside the library

A new MCU port does not automatically mean DAS should provide the application's startup code.

A hardware qualification target may include a minimal reset handler and linker script under `tests/hardware/<target>/`, but normal applications should remain free to provide their own startup, bootloader, RTOS, memory layout, and clock policy.

If architecture-level reusable startup support is added later, it should be a distinct optional layer rather than an accidental dependency of a peripheral library.

## 6. Add hardware qualification

A backend is not complete when compilation succeeds.

Create a physical-target test area under:

```text
tests/hardware/<target>/
```

and supporting scripts under:

```text
scripts/
scripts/gdb/
```

Qualification should test the public DAS API, not duplicate the backend's register writes in the test firmware.

GDB/register reads may be used as independent evidence, but where practical the test should include a physical path.

Examples:

- GPIO output: board LED or output pad observation;
- GPIO input: output-to-input jumper loopback;
- pull configuration: undriven input state;
- open drain: pull-up plus driven-low/released behavior;
- EXTI: physical output edge into an interrupt input;
- UART: TX-to-RX loopback;
- SPI: controller/peripheral loopback or known peripheral;
- DMA/cache paths: memory and peripheral evidence plus integrity counters.

## 7. Package test evidence

A hardware campaign should leave enough evidence to diagnose failures after the board is no longer in front of the developer.

The current STM32H755 campaign packages:

- build output;
- compiler/tool versions;
- repository commit IDs;
- OpenOCD log;
- per-case GDB logs;
- ELF;
- linker map;
- symbol table;
- summary and exit status.

New target campaigns should follow the same pattern where useful.

## Port acceptance checklist

Before considering a new target supported:

- [ ] public API compiles without vendor types leaking into headers;
- [ ] target selection is explicit in CMake;
- [ ] backend uses only documented external dependencies;
- [ ] board mappings match board documentation/schematic;
- [ ] library builds with warnings treated as errors;
- [ ] hardware test image boots independently;
- [ ] register/configuration evidence passes;
- [ ] physical I/O paths are exercised where feasible;
- [ ] failure evidence is preserved in a portable archive;
- [ ] documentation support matrix is updated;
- [ ] any unsupported API behavior is explicitly documented.

## Adding a new peripheral API

A new peripheral should normally be developed in this order:

1. define the generic public types and semantics;
2. decide what policy remains application-owned;
3. implement one backend;
4. add a board mapping only if the board has semantic resources for it;
5. build a physical qualification path;
6. add API/integration documentation;
7. only then replicate the backend on other MCU families.

This order keeps the generic API driven by real hardware rather than by an abstract taxonomy of every feature a vendor brochure has ever listed.
