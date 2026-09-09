# DAS — Device Abstraction Stack

DAS is a small, layered C library for embedded systems that separates application code from CPU/core architecture, device-specific peripherals, board wiring, and generated vendor projects.

The current STM32 path uses **CMSIS core/device definitions directly rather than STM32 HAL or LL**. The goal is not to replace one giant framework with another. DAS keeps hardware ownership explicit enough to inspect, port and physically qualify.

## Status

DAS is in early development. The current CMake project version is `0.1.0`.

| Area | Current support |
| --- | --- |
| Language | C11 |
| Build system | CMake 3.20+ |
| CPU/core architecture | Cortex-M, current target Cortex-M7 |
| Device | STM32H755 |
| Board | NUCLEO-H755ZI-Q |
| Vendor dependency | STM32CubeH7 CMSIS headers only |
| Cortex-M startup | reusable reset/runtime initialization and weak core exception defaults |
| STM32H755 linker | reusable CM7 memory layout exposed through `das::linker` |
| GPIO | input/output, pulls, push-pull/open-drain, speed, alternate-function configuration, EXTI |
| Board API | green/yellow/red user LEDs |
| Hardware qualification | OpenOCD + GDB + linker-map checks + physical wiring/visual confirmation |

Target selection is compile-time. There is no runtime hardware-discovery layer.

## Purpose

DAS is intended to make embedded firmware independent of bulky generated vendor projects while preserving direct control over the hardware.

The project aims for:

- stable application-facing C APIs;
- explicit compile-time target selection;
- CPU/core code separated from vendor device peripherals;
- device peripheral backends using CMSIS register definitions directly;
- board mappings for real physical wiring and named resources;
- reusable but optional startup/linker infrastructure;
- no required code generator;
- no mandatory heap, scheduler or RTOS;
- hardware behavior qualified on the real target rather than inferred from successful compilation.

The NUCLEO-H755ZI-Q target is being built toward a complete workflow based on CMake, ARM GNU, CMSIS, OpenOCD and GDB, without requiring CubeIDE/CubeMX-generated startup, linker, clock or peripheral-initialization files.

## Layers

```text
Application / RTOS
        |
        v
Public DAS API
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

Current layout:

```text
include/das/                     Public API
src/common/                      Hardware-independent shared logic
src/mcu/cortex_m/                Cortex-M-only core/runtime support
src/device/stm32h755/            STM32H755 on-chip peripheral backends
src/board/nucleo_h755zi_q/       NUCLEO physical mappings/resources
cmake/targets/                    Device-specific reusable link/memory policy
cmake/toolchains/                 Cross-compilation toolchains
examples/                        User-facing examples
scripts/                         Build/OpenOCD/GDB/qualification helpers
tests/hardware/                  Physical target qualification
docs/                            Architecture, integration and API documentation
```

STM32 peripheral register programming does **not** belong in `src/mcu/`. Cortex-M is the CPU architecture; GPIO, RCC and USART are STM32H755 device features.

## Current target composition

For:

```cmake
-DAS_DEVICE=nucleo_h755zi_q
```

CMake composes:

```text
MCU/core backend:  cortex_m
Device backend:    stm32h755
Board backend:     nucleo_h755zi_q
Linker default:    cmake/targets/stm32h755_cm7.ld
```

Current source implementation:

```text
src/mcu/cortex_m/startup.c
src/device/stm32h755/gpio.c
src/board/nucleo_h755zi_q/board.c
```

## Building

DAS currently expects an STM32CubeH7 checkout for CMSIS core and STM32H755 device headers. It does not compile or link STM32 HAL/LL sources.

```bash
cmake -S . -B build/stm32h755 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
  -DDAS_DEVICE=nucleo_h755zi_q \
  -DSTM32_CUBE_H7_DIR=/path/to/STM32CubeH7

cmake --build build/stm32h755 --parallel
```

The reusable code target is:

```cmake
das::das
```

The optional selected linker policy is:

```cmake
das::linker
```

A complete bare-metal firmware using the DAS default layout can link both:

```cmake
target_link_libraries(my_firmware PRIVATE
    das::das
    das::linker)
```

Applications with a bootloader or custom memory map can link only `das::das` and supply their own linker script, or override `DAS_LINKER_SCRIPT`.

See [Building and integration](docs/integration.md) and [STM32H755 memory layout](docs/memory-layout.md).

## Reusable Cortex-M startup

The Cortex-M layer provides weak reset/core-exception implementations. The default reset path:

1. restores `.data`;
2. clears `.bss`;
3. programs SCB VTOR;
4. executes DSB/ISB barriers;
5. calls `main()`.

A bootloader, RTOS or application can provide strong replacements. Device-specific external IRQ vectors remain owned by the final target image.

## Default STM32H755 memory layout

The reusable script:

```text
cmake/targets/stm32h755_cm7.ld
```

models the STM32H755 on-chip FLASH, ITCM, DTCM, AXI SRAM, SRAM1-4 and backup SRAM. The default placement uses flash for vectors/code and AXI SRAM for ordinary writable data, heap bounds and stack reservation.

It exports the symbols required by the Cortex-M startup path and is consumed through `das::linker`.

See [STM32H755 Cortex-M7 memory layout](docs/memory-layout.md) for addresses, sections, stack/heap boundaries and override mechanisms.

## Public APIs

Current public headers include:

- [`include/das/result.h`](include/das/result.h) — common result codes;
- [`include/das/gpio.h`](include/das/gpio.h) — GPIO configuration, I/O and EXTI support;
- [`include/das/board.h`](include/das/board.h) — named board resources;
- [`include/das/cortex_m/startup.h`](include/das/cortex_m/startup.h) — optional Cortex-M startup/core exception entry points.

See [API reference](docs/api.md).

## Hardware qualification

Run:

```bash
./scripts/stm32h755_test_campaign.sh \
  /path/to/STM32CubeH7 \
  --clean
```

The campaign now validates:

- reusable STM32H755 linker/map placement;
- Cortex-M7/OpenOCD identity and flash programming;
- reusable Cortex-M reset/runtime behavior;
- `.data` restoration and `.bss` clearing;
- VTOR/vector-table placement;
- GPIO clocks/configuration;
- pull-up/down;
- physical loopback;
- open-drain behavior;
- rising/falling EXTI delivery;
- user LED states and blinking.

A complete campaign contains **14 acceptance points** and packages the ELF, map, exact linker script, symbol table and all GDB/OpenOCD logs into a timestamped `.tar.gz` evidence archive.

See [Hardware qualification](docs/testing.md).

## Documentation

- [Architecture and layer rules](docs/architecture.md)
- [Building and integration](docs/integration.md)
- [STM32H755 Cortex-M7 memory layout](docs/memory-layout.md)
- [Public API reference](docs/api.md)
- [Porting DAS](docs/porting.md)
- [Hardware qualification](docs/testing.md)

## License

Apache License 2.0. See [LICENSE](LICENSE).
