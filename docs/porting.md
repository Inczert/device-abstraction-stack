# Porting DAS

DAS targets are composed from independent architecture, device, board and build-policy pieces.

```text
MCU/core architecture
        +
silicon device
        +
board
        +
selected CPU/core on multi-core devices
        =
DAS build target
```

Current example:

```text
Cortex-M + STM32H755 + NUCLEO-H755ZI-Q + CM7/CM4
```

## MCU/core architecture layer

Location:

```text
src/mcu/<architecture>/
```

The layer contains architecture-only behavior such as:

- reset/runtime mechanics;
- core exceptions;
- NVIC/SCB;
- core timers such as SysTick;
- cache/MPU primitives;
- architectural barriers/masking.

It must not contain vendor peripherals.

A multi-core device may contain two cores from the same architecture family. In that case shared architecture code should remain shared rather than being copied into device-specific directories.

## Device layer

Location:

```text
src/device/<device>/
```

This layer owns on-chip device behavior.

For STM32H755 that includes GPIO, RCC/PWR/FLASH, EXTI/SYSCFG, USART, DMA, timers, SPI/I2C, ADC, watchdog, HSEM and other silicon features.

Device code may use vendor CMSIS device headers internally.

For multi-core silicon, check whether registers have CPU-specific views. A backend must not silently use CPU1 registers when it is being compiled for CPU2.

## Board layer

Location:

```text
src/board/<board>/
```

The board layer describes physical resources and wiring:

- LEDs/buttons;
- connector functions;
- VCOM;
- fixed enables/chip selects;
- board oscillators;
- onboard sensors/transceivers.

It should consume device/public APIs rather than duplicate register programming.

## Core selection

If one device supports multiple executable cores, add an explicit build selector rather than encoding the core into the board name.

For STM32H755:

```text
DAS_DEVICE=nucleo_h755zi_q
DAS_CORE=cm7 | cm4
```

Core selection may control:

- compiler CPU/FPU options;
- CMSIS core header;
- device preprocessor core macro;
- core-specific device register views;
- default final-image linker script.

Use separate CMake build directories for different cores.

## Linker/build policy

Physical flash/RAM addresses do not belong in generic CPU code.

Place default target linker scripts under:

```text
cmake/targets/
```

A device/core pair may have different defaults:

```text
stm32h755_cm7.ld
stm32h755_cm4.ld
```

The static DAS library should remain the consumer-facing target. Linker policy can be propagated transitively from `das::das` to the final executable.

Do not create pseudo-library names for linker scripts.

Provide a single override such as:

```text
DAS_LINKER_SCRIPT=/path/to/custom.ld
```

so applications with bootloaders, RTOS layouts, external RAM or custom partitions remain supported.

A reusable linker script that uses the DAS Cortex-M startup should export the startup-symbol contract documented in `docs/memory-layout.md`.

## CMake composition

A target should resolve its pieces explicitly. Conceptually:

```cmake
DAS_DEVICE -> device + board
DAS_CORE   -> CPU flags + CMSIS core + core macro + linker default
```

Only selected target sources should be compiled.

As the target matrix grows, the composition may move into dedicated CMake modules, but the dependency graph must remain visible.

## External dependencies

For Cortex-M targets, prefer:

- CMSIS-Core for architectural definitions;
- vendor CMSIS device headers for register definitions.

Document:

- required checkout/package;
- include paths;
- preprocessor symbols;
- compiler ABI/FPU flags;
- whether any vendor source files are linked.

Avoid importing an entire vendor SDK if register definitions are sufficient.

## Startup and vector tables

Reusable architecture startup belongs in `src/mcu/`.

Device-specific external IRQ numbering and the final vector table belong to the concrete target image.

A custom application may replace the DAS weak startup and use a different linker contract.

## Hardware qualification

Compilation is not sufficient for a hardware backend.

Qualification should combine, where possible:

- register/configuration evidence;
- runtime execution evidence;
- physical I/O evidence.

For a multi-core device distinguish:

1. **static core-image qualification**: compiler flags, linker placement, symbol contract;
2. **physical core qualification**: reset/release, execution, interrupts;
3. **dual-core qualification**: shared memory, HSEM, inter-core signaling and coordinated lifecycle.

This prevents a basic linker issue from absorbing the entire multi-core bring-up scope.

## Evidence bundles

Hardware campaigns should preserve:

- build logs;
- compiler/CMake/OpenOCD/GDB versions;
- source revision;
- external CMSIS revision when available;
- linker scripts;
- ELF/map files;
- symbol tables;
- per-test debugger logs;
- summary and exit status.

## Port acceptance checklist

Before marking a new target supported:

- [ ] public API remains vendor-type free;
- [ ] architecture layer contains no vendor peripheral code;
- [ ] device layer contains no board assumptions;
- [ ] board layer does not duplicate register backends;
- [ ] core selection is explicit where required;
- [ ] compiler/core ABI flags are correct;
- [ ] linker/memory policy is documented and overridable;
- [ ] startup/linker symbol contract is satisfied;
- [ ] host/cross-build validation passes;
- [ ] physical behavior is tested where feasible;
- [ ] evidence is packaged;
- [ ] documentation/support matrix is updated.

## Adding a peripheral

Recommended order:

1. define generic public semantics;
2. decide what policy remains application-owned;
3. implement one device backend;
4. add architecture helpers only for genuinely architectural behavior;
5. add board mappings only for real physical board semantics;
6. add static and physical qualification;
7. update API/integration documentation;
8. replicate the backend on other devices.
