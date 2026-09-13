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

This layer contains architecture-only behavior such as reset/runtime mechanics, core exceptions, NVIC/SCB operations, core timers, cache/MPU primitives and architectural barriers/masking. It must not contain vendor peripherals.

A multi-core device may contain multiple cores from the same architecture family. Shared architecture code should remain shared rather than being copied into device-specific directories.

## Device layer

Location:

```text
src/device/<device>/
```

This layer owns on-chip silicon behavior. For the current STM32H755 implementation that includes GPIO/EXTI, RCC/PWR/FLASH clocking, USART, SPI, I2C, timer/PWM, generic DMA1/DMAMUX1, the Ethernet MAC/dedicated Ethernet DMA/MDIO path, and the device vector layout.

Future device services such as ADC, watchdog, internal flash/reset-cause handling and HSEM/shared-resource support also belong in this layer when implemented. Listing a silicon feature here does not make it part of the current supported API.

Device code may use vendor CMSIS device headers internally. For multi-core silicon, check whether registers have CPU-specific views; a backend must not silently use CPU1 registers when compiled for CPU2.

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
- board oscillators/power policy;
- onboard PHYs, sensors or transceivers;
- semantic peripheral routes such as the NUCLEO-H755ZI-Q RJ45/LAN8742A path.

It should consume generic/device functionality rather than duplicate register backends.

## Core selection

If one device supports multiple executable cores, use an explicit selector rather than encoding the core into the board name.

For STM32H755:

```text
DAS_DEVICE=nucleo_h755zi_q
DAS_CORE=cm7 | cm4
```

Core selection may control compiler CPU/FPU options, CMSIS core definitions, device preprocessor macros, core-specific register views and default linker layout. Use separate build directories for different cores.

## Linker/build policy

Physical flash/RAM addresses do not belong in generic CPU code. Put default target linker scripts under `cmake/targets/` and propagate the selected script through the normal `das::das` target.

Provide one application override:

```text
DAS_LINKER_SCRIPT=/path/to/custom.ld
```

so bootloaders, RTOS layouts, external RAM, custom partitions and specialized DMA/shared-memory placement remain possible.

A custom linker using DAS Cortex-M startup must export the contract documented in `docs/memory-layout.md`. Integrations may require additional bounds such as `__RAM_START__` / `__RAM_END__`. Peripheral-specific DMA engines also impose physical reachability constraints; for example, STM32H755 Ethernet descriptors/buffers cannot be placed in DTCM.

## CMake composition

A target should resolve its pieces explicitly:

```cmake
DAS_DEVICE -> device + board
DAS_CORE   -> CPU flags + CMSIS core + core macro + linker default
```

Only selected target sources should be compiled. As the target matrix grows, composition may move into dedicated CMake modules, but the dependency graph should remain visible.

## External dependencies

For Cortex-M targets, prefer CMSIS-Core for architectural definitions and vendor CMSIS device headers for register definitions. Document required revisions, include paths, preprocessor symbols, compiler ABI/FPU flags and whether any vendor source files are linked.

Avoid importing an entire vendor SDK when register definitions are sufficient.

## Startup and vector tables

Reusable reset/runtime mechanics belong in `src/mcu/`. Device-specific IRQ numbering and canonical device-vector layout belong with the concrete device/target implementation.

For the STM32H755 model, normal firmware keeps the DAS-owned canonical table and overrides individual weak standard handler symbols with strong application/driver/RTOS definitions. A port should prefer this model when the device has a fixed vector layout and the ownership semantics remain clear.

Whole-table replacement must remain explicit for bootloaders or specialized runtimes rather than becoming a prerequisite for ordinary peripheral ISR binding.

## Ethernet/network-stack boundary

A hardware Ethernet port should separate Layer-2 device ownership from higher protocol policy:

```text
application / optional network stack
        ↓
portable DAS Layer-2 API
        ↓
device MAC/DMA backend
        ↓
board PHY/connector route
```

Do not leak lwIP, socket, PHY-vendor or MAC-register types into the generic public API merely because one target uses them internally.

Likewise, do not assume a generic MCU DMA API is the correct abstraction for a peripheral that owns a dedicated DMA engine, such as STM32H755 Ethernet.

## Hardware qualification

Compilation is not sufficient for a hardware backend. Qualification should combine register/configuration evidence, runtime execution evidence and physical I/O evidence where practical.

For multi-core devices distinguish:

1. **static core-image qualification**: compiler flags, linker placement, symbols;
2. **physical core qualification**: real-core execution and interrupts;
3. **dual-core system qualification**: lifecycle, shared memory, HSEM/inter-core signaling and coordinated ownership.

Do not quietly upgrade level 2 evidence into level 3 claims. The debugger is talented, but not magical.

For network devices, physical qualification should additionally cover link state and bidirectional traffic, plus cache/DMA ownership when relevant. Link-down/up transitions should be tested separately when they are part of the claimed behavior.

## Evidence bundles

Hardware campaigns should preserve:

- build logs;
- compiler/CMake/OpenOCD/GDB versions;
- source revision;
- external CMSIS revision;
- linker scripts;
- ELF/map files;
- symbol tables;
- per-test debugger/traffic logs;
- fixture/configuration metadata;
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
- [ ] startup/vector/linker ownership is explicit;
- [ ] DMA-visible placement constraints are documented where applicable;
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

The current NUCLEO-H755ZI-Q reference campaign is **39/39 PASS** at DAS commit `f6b65672d9ae69cf28cd574d0dbba01cf875d8dc`; it is an example of the evidence standard, not a promise that future ports inherit qualification by resemblance.
