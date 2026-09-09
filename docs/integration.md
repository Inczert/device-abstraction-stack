# Building and integration

This document describes how DAS selects and composes a target, how the static library is built, and how another firmware project consumes it.

## Requirements

For the current STM32H755 target:

- CMake 3.20 or newer;
- ARM GNU bare-metal toolchain (`arm-none-eabi-gcc`);
- an STM32CubeH7 checkout containing CMSIS core and STM32H755 device headers;
- C11 support.

For hardware qualification:

- OpenOCD;
- `gdb-multiarch` or `arm-none-eabi-gdb`;
- ST-LINK access to a NUCLEO-H755ZI-Q;
- one jumper wire for GPIO loopback cases.

DAS does not require STM32 HAL or LL libraries.

## Target selection and composition

The public target selector is currently:

```text
DAS_DEVICE=nucleo_h755zi_q
```

Internally this resolves to three independent implementation layers:

```text
MCU/core backend = cortex_m
Device backend   = stm32h755
Board backend    = nucleo_h755zi_q
```

The selected target therefore compiles:

```text
src/device/stm32h755/gpio.c
src/board/nucleo_h755zi_q/board.c
```

The Cortex-M source list is currently empty; upcoming core-only startup, NVIC, SysTick and cache/MPU support will be added under `src/mcu/cortex_m/`.

CMake fails explicitly for unknown target values.

## Layer-specific dependencies

The current STM32 target uses two CMSIS boundaries:

- CMSIS-Core for Cortex-M architectural definitions;
- the STM32H755 CMSIS device header for silicon peripheral registers.

STM32 peripheral code belongs in `src/device/stm32h755/`, not `src/mcu/`.

Board code belongs in `src/board/nucleo_h755zi_q/` and should not program STM32 registers directly when a device/public API exists.

## STM32CubeH7 dependency

Pass the STM32CubeH7 root as:

```bash
-DSTM32_CUBE_H7_DIR=/path/to/STM32CubeH7
```

DAS searches for CMSIS-Core in either:

```text
Drivers/CMSIS/Core/Include
Drivers/CMSIS/Include
```

and the STM32H755 device header under:

```text
Drivers/CMSIS/Device/ST/STM32H7xx/Include
```

No STM32 HAL/LL source files are linked.

## Standalone library build

```bash
cmake -S . -B build/stm32h755 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDAS_DEVICE=nucleo_h755zi_q \
  -DSTM32_CUBE_H7_DIR=/path/to/STM32CubeH7

cmake --build build/stm32h755 --parallel
```

The configure output reports the composition:

```text
DAS composition: mcu=cortex_m, device=stm32h755, board=nucleo_h755zi_q
```

The static target is:

```text
das
```

with alias:

```text
das::das
```

The archive is normally:

```text
build/stm32h755/libdas.a
```

DAS compiles its own library sources with `-Wall -Wextra -Werror`.

## Cross-compilation toolchain

The repository provides:

```text
cmake/toolchains/arm-none-eabi.cmake
```

for the current Cortex-M7 bare-metal build.

A consuming application may use its own equivalent toolchain file. DAS must be configured in the same cross-compilation build tree as the firmware using it.

## `add_subdirectory()` integration

Example layout:

```text
my-firmware/
├── CMakeLists.txt
├── src/
└── third_party/
    └── device-abstraction-stack/
```

Parent CMake:

```cmake
set(DAS_DEVICE nucleo_h755zi_q CACHE STRING "DAS target" FORCE)
set(STM32_CUBE_H7_DIR "/path/to/STM32CubeH7" CACHE PATH "STM32CubeH7 root" FORCE)

add_subdirectory(third_party/device-abstraction-stack)

target_link_libraries(my_firmware PRIVATE das::das)
```

Application source then uses public headers:

```c
#include <das/gpio.h>
#include <das/board.h>
```

Do not add `src/mcu/`, `src/device/`, or `src/board/` to application include paths. Those are implementation layers, not public interfaces.

## `FetchContent` integration

```cmake
include(FetchContent)

set(DAS_DEVICE nucleo_h755zi_q CACHE STRING "DAS target" FORCE)
set(STM32_CUBE_H7_DIR "/path/to/STM32CubeH7" CACHE PATH "STM32CubeH7 root" FORCE)

FetchContent_Declare(
    das
    GIT_REPOSITORY https://github.com/Inczert/device-abstraction-stack.git
    GIT_TAG        develop
)

FetchContent_MakeAvailable(das)

target_link_libraries(my_firmware PRIVATE das::das)
```

Use a release tag or exact commit for reproducible products. `develop` is appropriate only when intentionally tracking active development.

## Installed-package status

DAS does not yet provide an installed `find_package(DAS)` package. Source integration currently uses `add_subdirectory()` or `FetchContent`.

Install/export support is tracked separately and should preserve the same core/device/board composition model.

## Compile definitions

For the current target DAS privately defines:

```text
CORE_CM7
STM32H755xx
```

These are implementation requirements. Consuming application code must not depend on them being propagated by `das::das`.

## What the application still owns

Linking DAS does not create a complete firmware image. At the current stage the consuming firmware owns:

- startup/reset entry;
- vector table;
- linker script and memory layout;
- system clock configuration;
- C/C++ runtime setup as required;
- application `main` or RTOS entry;
- NVIC priority/vector policy;
- bootloader/application partitioning.

Planned work will provide optional reusable Cortex-M startup/core helpers and STM32H755 linker/clock support. Those additions must remain explicit and overridable rather than silently becoming mandatory application policy.

## Hardware-test build

Enable the qualification firmware with:

```bash
-DDAS_BUILD_HARDWARE_TESTS=ON
```

or use:

```bash
./scripts/build_stm32h755.sh /path/to/STM32CubeH7 --clean
```

The hardware-test ELF is:

```text
build/stm32h755/tests/hardware/stm32h755/das_stm32h755_hw_test.elf
```

The test image currently supplies its own minimal startup and linker script.

## Full physical campaign

```bash
./scripts/stm32h755_test_campaign.sh \
  /path/to/STM32CubeH7 \
  --clean
```

The campaign builds, starts OpenOCD, flashes through GDB, performs automated register/physical GPIO checks, asks for visual LED confirmation, and packages a timestamped evidence archive.

See [Hardware qualification](testing.md).

## Configuration summary

| CMake variable | Required | Meaning |
| --- | --- | --- |
| `DAS_DEVICE` | yes | Compile-time target selector; currently `nucleo_h755zi_q` |
| `STM32_CUBE_H7_DIR` | yes for current target | STM32CubeH7 root used for CMSIS headers |
| `DAS_BUILD_HARDWARE_TESTS` | no | Build the physical qualification firmware; default `OFF` |
| `CMAKE_TOOLCHAIN_FILE` | required for standalone ARM cross-build | ARM bare-metal toolchain |
| `CMAKE_BUILD_TYPE` | no | Standard CMake build type |
