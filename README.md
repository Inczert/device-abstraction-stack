# DAS — Device Abstraction Stack

DAS is a small, layered C library for embedded systems. It separates application-facing APIs from CPU architecture code, silicon-specific peripheral code, board wiring, and final firmware build policy.

The current STM32 path uses **CMSIS definitions directly**, without STM32 HAL/LL or CubeIDE/CubeMX-generated initialization files.

## Current status

| Area | Support |
| --- | --- |
| Language | C11 |
| Build | CMake 3.20+ / `arm-none-eabi-gcc` |
| Architecture | Cortex-M |
| Device | STM32H755 |
| Board | NUCLEO-H755ZI-Q |
| Cores | CM7 build/physical qualification; CM4 build/link-layout qualification |
| Startup | reusable weak Cortex-M reset/runtime path |
| Linker | default STM32H755 CM7 and CM4 layouts, overridable |
| GPIO | input/output, pulls, push-pull/open-drain, AF configuration, EXTI |
| Board API | green/yellow/red user LEDs |
| Debug/test | OpenOCD + GDB + packaged evidence campaign |

DAS is still early development. The current project version is `0.1.0`.

## Purpose

The goal is to build practical embedded firmware without requiring a bulky generated vendor project while preserving direct, inspectable control over the hardware.

DAS aims for:

- stable public C APIs;
- compile-time target selection;
- explicit CPU/core selection on multi-core devices;
- Cortex-M code separated from STM32 peripheral code;
- board wiring separated from device registers;
- no required code generator;
- no mandatory heap, RTOS or scheduler;
- reusable startup/linker defaults that applications can replace;
- physical qualification for supported hardware behavior.

## Layer model

```text
Application / RTOS
        |
        v
Public DAS API
 include/das/
        |
   +----+----+
   |         |
   v         v
common     board
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
```

Implementation ownership:

```text
src/mcu/cortex_m/
    Cortex-M architecture only
    startup, future NVIC/SysTick/cache/MPU

src/device/stm32h755/
    STM32H755 silicon/peripherals
    GPIO/EXTI now, RCC/UART/DMA/etc. later

src/board/nucleo_h755zi_q/
    physical Nucleo resources
    LEDs now, button/VCOM/connectors/etc. later

cmake/targets/
    final firmware memory/linker policy
```

See [Architecture](docs/architecture.md).

## Target and core selection

The current board is selected with:

```cmake
-DAS_DEVICE=nucleo_h755zi_q
```

STM32H755 contains two CPUs, so the core is selected separately.

CM7:

```cmake
-DDAS_CORE=cm7
```

CM4:

```cmake
-DDAS_CORE=cm4
```

The selected core controls:

- CPU/FPU compiler flags;
- CMSIS core header;
- `CORE_CM7` / `CORE_CM4`;
- default linker script.

The default core is `cm7`.

## Building

Example CM7 configuration:

```bash
cmake -S . -B build/cm7 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDAS_DEVICE=nucleo_h755zi_q \
  -DDAS_CORE=cm7 \
  -DSTM32_CUBE_H7_DIR=/path/to/STM32CubeH7

cmake --build build/cm7 --parallel
```

CM4 uses the same command with a separate build directory and:

```text
-DDAS_CORE=cm4
```

DAS requires STM32CubeH7 only for CMSIS core/device headers. HAL and LL sources are not linked.

## Using DAS in firmware

A normal parent project needs only the DAS library target:

```cmake
set(DAS_DEVICE nucleo_h755zi_q CACHE STRING "" FORCE)
set(DAS_CORE cm7 CACHE STRING "" FORCE)
set(STM32_CUBE_H7_DIR "/path/to/STM32CubeH7" CACHE PATH "" FORCE)

add_subdirectory(third_party/device-abstraction-stack)

add_executable(my_firmware src/main.c)
target_link_libraries(my_firmware PRIVATE das::das)
```

There is no separate `das::linker` target.

`libdas.a` is a static archive and has no final physical addresses by itself. The selected linker script is propagated through `das::das` and applies when `my_firmware` is linked into an ELF.

## Default linker layouts

DAS provides:

```text
cmake/targets/stm32h755_cm7.ld
cmake/targets/stm32h755_cm4.ld
```

The dual-core-safe defaults are:

```text
CM7
  flash bank 1 : 0x08000000..0x080FFFFF
  AXI SRAM     : 0x24000000..0x2407FFFF

CM4
  flash bank 2 : 0x08100000..0x081FFFFF
  D2 SRAM1     : 0x30000000..0x3001FFFF
```

Override the selected default with:

```bash
-DDAS_LINKER_SCRIPT=/path/to/custom.ld
```

An empty `DAS_LINKER_SCRIPT` uses the DAS device/core default.

See [STM32H755 memory and linker policy](docs/memory-layout.md).

## Reusable Cortex-M startup

The Cortex-M layer provides a weak reset/runtime path that:

1. restores `.data`;
2. clears `.bss`;
3. sets VTOR;
4. executes the required barriers;
5. calls `main()`.

Applications with a bootloader, RTOS or custom startup can replace the weak symbols.

Device-specific external IRQ vectors remain part of the final target image.

## Public APIs

Current public headers:

```text
include/das/result.h
include/das/gpio.h
include/das/board.h
include/das/cortex_m/startup.h
```

See [API reference](docs/api.md).

## Hardware qualification

Run the full STM32H755 campaign:

```bash
./scripts/stm32h755_test_campaign.sh \
    /home/dev/STM32Cube/Repository/STM32CubeH7/ \
    --clean
```

The campaign:

- statically validates the CM7 linker layout;
- builds and statically validates an independent CM4 linker-smoke image;
- probes the real Cortex-M7 before flashing;
- validates CM7 flash/startup/runtime behavior;
- tests GPIO pulls, loopback, open-drain and EXTI;
- tests the three Nucleo user LEDs;
- packages logs, ELF/map files, symbol tables and linker scripts into a `.tar.gz`.

The current successful target is **15 acceptance points**.

CM4 physical boot/release is intentionally separate from linker qualification and is tracked by the dual-core work.

See [Hardware qualification](docs/testing.md).

## Documentation

- [Architecture](docs/architecture.md)
- [Building and integration](docs/integration.md)
- [STM32H755 memory/linker policy](docs/memory-layout.md)
- [Public API reference](docs/api.md)
- [Porting DAS](docs/porting.md)
- [Hardware qualification](docs/testing.md)

## License

Apache License 2.0. See [LICENSE](LICENSE).
