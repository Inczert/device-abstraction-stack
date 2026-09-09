# Porting DAS

DAS grows by composing independent CPU/core, device, board, and build-time memory-policy layers behind stable public APIs. A port should preserve those boundaries instead of introducing vendor-specific types into application code.

## Porting model

```text
MCU/core architecture
        +
Device/silicon implementation
        +
Board mapping
        +
Device memory/link policy
        =
DAS target
```

Current target:

```text
Cortex-M
+ STM32H755
+ NUCLEO-H755ZI-Q
+ STM32H755 CM7 default linker layout
= DAS_DEVICE=nucleo_h755zi_q
```

## 1. Add or reuse the MCU/core layer

Location:

```text
src/mcu/<architecture>/
```

This layer contains CPU/core architecture support only, in C and/or assembly: exception mechanics, core interrupt helpers, architectural timers, barriers, cache/MPU primitives, and reusable startup mechanics.

It must not contain vendor peripheral implementations such as STM32 GPIO, RCC, USART, DMA, timers, SPI, I2C or ADC.

## 2. Add the device layer

Location:

```text
src/device/<device>/
```

This layer implements silicon-specific peripherals using the selected device's register definitions. Device headers may be used internally, but vendor types must not leak into public DAS headers.

A device implementation should validate arguments, configure only the hardware it owns, avoid board policy, use atomic operations where available, avoid hidden heap/RTOS dependencies, and return explicit errors/timeouts instead of hanging indefinitely.

## 3. Add the board layer

Location:

```text
src/board/<board>/
```

The board layer maps physical resources such as LEDs, buttons, connector buses, VCOM, fixed enables and clock-source wiring onto generic/device capabilities.

Do not use the board layer as a second register backend or as a one-for-one alias table for every MCU pin.

## 4. Add device/build memory policy

Physical flash/RAM addresses belong to the silicon target, not the CPU architecture. Reusable linker scripts therefore live under:

```text
cmake/targets/
```

For STM32H755 CM7:

```text
cmake/targets/stm32h755_cm7.ld
```

A new device port should define a default memory layout only when DAS can state it clearly and qualify it. The script should export the symbols required by the selected startup implementation and expose useful stack/heap/noinit boundaries.

Keep the linker policy optional. A bootloader, RTOS, bank-partitioned image or special memory-placement application must be able to omit the DAS linker target and use its own script.

## 5. Extend CMake composition

The selected public target should resolve explicitly to its implementation pieces and default linker script.

Conceptually:

```cmake
if(DAS_DEVICE STREQUAL "nucleo_h755zi_q")
    set(DAS_MCU_BACKEND cortex_m)
    set(DAS_DEVICE_BACKEND stm32h755)
    set(DAS_BOARD_BACKEND nucleo_h755zi_q)
    set(DAS_DEFAULT_LINKER_SCRIPT .../stm32h755_cm7.ld)
endif()
```

DAS keeps code and link policy separate:

```text
das::das       reusable implementation
das::linker    optional selected linker script
```

Only sources required for the selected target should be compiled.

## 6. Define low-level dependencies

For Cortex-M targets CMSIS is the preferred boundary where practical:

- CMSIS-Core for CPU/core definitions;
- vendor CMSIS device headers for device registers.

Document required headers, root paths, preprocessor symbols, ABI/toolchain constraints and whether vendor source files are linked. Avoid importing a complete SDK when register definitions are sufficient.

## 7. Keep responsibilities separated

```text
CPU startup mechanics       -> src/mcu/<architecture>/
Peripheral implementation   -> src/device/<device>/
Board wiring                -> src/board/<board>/
Flash/RAM/linker layout      -> cmake/targets/
```

Applications remain free to override startup and linker behavior for bootloaders, RTOSes, custom partitions and vector placement.

## 8. Add qualification

A port is not complete because it compiles.

Create physical qualification under:

```text
tests/hardware/<target>/
```

Qualification should combine build/link evidence, execution state and physical behavior. Examples include:

- linker layout: ELF/map symbol/address checks;
- startup: dirty `.data`/`.bss`, reset, verify restoration;
- GPIO input/output: physical loopback;
- pulls: undriven input;
- EXTI: physical edge into an interrupt input;
- UART/SPI: loopback;
- timers/PWM: capture or external measurement;
- DMA: pattern integrity plus completion/error evidence.

## 9. Package evidence

Hardware campaigns should preserve enough information to diagnose failures after the target is disconnected. The STM32H755 campaign packages build/tool metadata, the exact linker script, ELF, linker map, symbol table, OpenOCD log, per-case logs, summary and exit status.

## Port acceptance checklist

Before marking a target supported:

- [ ] public API remains vendor-type free;
- [ ] core code contains no vendor peripheral implementation;
- [ ] device code contains no board wiring assumptions;
- [ ] board code does not duplicate device register programming;
- [ ] CMake composes core/device/board layers explicitly;
- [ ] default memory/link policy is documented or intentionally absent;
- [ ] custom linker/startup remains possible;
- [ ] external dependencies and ABI constraints are documented;
- [ ] warnings are treated as errors;
- [ ] target firmware boots independently;
- [ ] linker/startup/configuration evidence passes;
- [ ] physical I/O is exercised where feasible;
- [ ] evidence is archived;
- [ ] documentation/support matrix is updated.

## Adding a new peripheral API

Recommended order:

1. define generic public types and semantics;
2. decide what policy remains application-owned;
3. implement one device backend;
4. add MCU/core helpers only when genuinely architectural;
5. add board mappings only for real board semantics;
6. build a physical qualification path;
7. update API/integration documentation;
8. replicate the backend on other silicon.

This keeps the public API driven by real hardware behavior rather than vendor brochure feature lists.
