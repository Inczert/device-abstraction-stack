# Building and integration

This document describes how DAS selects a target, how it chooses the CPU core on a multi-core device, and how another firmware project consumes the library.

## Requirements

For the current STM32H755 target:

- CMake 3.20 or newer;
- ARM GNU bare-metal toolchain (`arm-none-eabi-gcc`);
- an STM32CubeH7 checkout containing CMSIS core and STM32H755 device headers;
- C11 support.

DAS uses CMSIS definitions only. It does not compile or link STM32 HAL or LL source files.

For the full dual-core hardware campaign, OpenOCD must provide the ST-LINK direct-DAP interface script (`interface/stlink-dap.cfg`) and STM32H7 dual-core support.

## Target composition

The current board selector is:

```text
DAS_DEVICE=nucleo_h755zi_q
```

Because STM32H755 contains two Cortex-M cores, the core is selected separately:

```text
DAS_CORE=cm7
```

or:

```text
DAS_CORE=cm4
```

The resulting composition is:

```text
MCU/core architecture = cortex_m
CPU core              = cm7 or cm4
Device                = stm32h755
Board                 = nucleo_h755zi_q
```

`DAS_CORE` selects CPU/FPU compiler flags, CMSIS core header, `CORE_CM7`/`CORE_CM4`, and the default STM32H755 linker script. The default remains `cm7`.

## Standalone build

CM7:

```bash
cmake -S . -B build/cm7 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDAS_DEVICE=nucleo_h755zi_q \
  -DDAS_CORE=cm7 \
  -DSTM32_CUBE_H7_DIR=/path/to/STM32CubeH7

cmake --build build/cm7 --parallel
```

CM4 uses a separate build directory and `-DDAS_CORE=cm4`.

The helper script can build either physical-test image:

```bash
./scripts/build_stm32h755.sh /path/to/STM32CubeH7 --core cm7 --clean
./scripts/build_stm32h755.sh /path/to/STM32CubeH7 --core cm4 --build-dir build/stm32h755/cm4-hw --clean
```

Use separate CMake build directories for the two cores. Their compiler flags, CMSIS definitions and linker policy differ.

## Consuming DAS

With `add_subdirectory()`:

```cmake
set(DAS_DEVICE nucleo_h755zi_q CACHE STRING "" FORCE)
set(DAS_CORE cm7 CACHE STRING "" FORCE)
set(STM32_CUBE_H7_DIR "/path/to/STM32CubeH7" CACHE PATH "" FORCE)

add_subdirectory(third_party/device-abstraction-stack)

add_executable(my_firmware src/main.c)
target_link_libraries(my_firmware PRIVATE das::das)
```

That final line is the normal integration contract. Application code includes public headers only; do not add `src/mcu/`, `src/device/`, or `src/board/` to application include paths.

## Static library versus linker script

`das::das` is a static library. The archive itself is not linked to a physical address. The selected `.ld` file is used when the final executable is created:

```text
application objects + libdas.a
              |
              v
       final executable link
       with selected .ld
              |
              v
         firmware.elf
```

DAS propagates the selected linker script from `das::das` through CMake `INTERFACE` link options. Therefore the consumer does not link a second pseudo-library and does not call a configuration helper:

```cmake
target_link_libraries(my_firmware PRIVATE das::das)
```

is sufficient for the default bare-metal layout.

## Linker selection and override

When `DAS_LINKER_SCRIPT` is empty, DAS selects:

```text
DAS_CORE=cm7 -> cmake/targets/stm32h755_cm7.ld
DAS_CORE=cm4 -> cmake/targets/stm32h755_cm4.ld
```

Override it with:

```cmake
set(DAS_LINKER_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/custom.ld"
    CACHE FILEPATH "" FORCE)
```

or:

```bash
-DDAS_LINKER_SCRIPT=/path/to/custom.ld
```

The hardware campaign contains a separate custom-link smoke build using `tests/link/stm32h755/custom_cm7.ld`. Its vector table is deliberately relocated to `0x08020000`, and the generated ELF/map are checked. This validates the override path independently of the default linker scripts.

See [STM32H755 memory and linker policy](memory-layout.md).

## Reusable Cortex-M startup

DAS includes optional weak Cortex-M startup definitions in:

```text
src/mcu/cortex_m/startup.c
include/das/cortex_m/startup.h
```

The default reset path restores `.data`, clears `.bss`, writes SCB VTOR, executes DSB/ISB, and calls `main()`.

Both default STM32H755 linker scripts provide the required startup symbols. A bootloader, RTOS or application may replace the weak startup handlers and/or the linker script.

The generic Cortex-M layer does not define STM32 external-IRQ vectors.

## Core-specific STM32 device definitions

For the selected core DAS privately defines either:

```text
STM32H755xx + CORE_CM7
```

or:

```text
STM32H755xx + CORE_CM4
```

The STM32H755 device backend uses the correct per-core RCC/EXTI view where the silicon exposes one. These definitions are implementation details, not public API.

## Dual-core hardware qualification

The full campaign builds both core images and then starts a **single** OpenOCD instance in direct-DAP dual-core mode:

```text
GDB :3333 -> CM7 / CPU1
GDB :3334 -> CM4 / CPU2
```

The campaign physically executes startup and automated GPIO/EXTI tests on both cores. It does not yet represent the production dual-core boot contract: CM4 execution is driven by the debugger, while CM7-to-CM4 boot/release, HSEM and shared-memory coordination remain separate system features.

The conservative single-core/HLA OpenOCD configuration remains available for explicit recovery operations.

## `FetchContent`

```cmake
include(FetchContent)

set(DAS_DEVICE nucleo_h755zi_q CACHE STRING "" FORCE)
set(DAS_CORE cm7 CACHE STRING "" FORCE)
set(STM32_CUBE_H7_DIR "/path/to/STM32CubeH7" CACHE PATH "" FORCE)

FetchContent_Declare(
    das
    GIT_REPOSITORY https://github.com/Inczert/device-abstraction-stack.git
    GIT_TAG        develop
)

FetchContent_MakeAvailable(das)
target_link_libraries(my_firmware PRIVATE das::das)
```

Use a release tag or fixed commit for reproducible products.

## CMake variables

| Variable | Default | Meaning |
| --- | --- | --- |
| `DAS_DEVICE` | `nucleo_h755zi_q` | selected board/target |
| `DAS_CORE` | `cm7` | selected CPU core (`cm7` or `cm4`) |
| `DAS_LINKER_SCRIPT` | empty | custom linker override; empty selects device/core default |
| `STM32_CUBE_H7_DIR` | empty | STM32CubeH7 root for CMSIS headers |
| `DAS_BUILD_LINK_TESTS` | `OFF` | build linker-smoke target |
| `DAS_BUILD_HARDWARE_TESTS` | `OFF` | build physical qualification firmware for the selected core |

## Installed-package status

DAS does not yet provide an installed `find_package(DAS)` package. Source integration currently uses `add_subdirectory()` or `FetchContent`.
