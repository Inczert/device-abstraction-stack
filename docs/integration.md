# Building and integration

This document describes how DAS is selected for a target, built as a static library, and included in another firmware project.

## Requirements

For the currently supported STM32H755 target:

- CMake 3.20 or newer;
- an ARM GNU bare-metal toolchain (`arm-none-eabi-gcc`);
- an STM32CubeH7 checkout containing CMSIS core and STM32H755 device headers;
- C11 support.

For physical hardware qualification you additionally need:

- OpenOCD;
- `gdb-multiarch` or `arm-none-eabi-gdb`;
- ST-LINK access to a NUCLEO-H755ZI-Q;
- a jumper wire for the GPIO loopback cases.

DAS itself does not require STM32 HAL or LL libraries.

## Target selection

The target is selected at configure time with `DAS_DEVICE`.

Current value:

```text
nucleo_h755zi_q
```

Example:

```bash
-DAS_DEVICE=nucleo_h755zi_q
```

The current implementation supports one device only. CMake fails explicitly for unknown values rather than silently compiling a mismatched backend.

The selected device determines the board layer, MCU-family backend, CMSIS device symbols, and external header requirements.

## STM32CubeH7 dependency

Pass the root of an STM32CubeH7 checkout as:

```bash
-DSTM32_CUBE_H7_DIR=/path/to/STM32CubeH7
```

DAS searches for the CMSIS core header in either:

```text
Drivers/CMSIS/Core/Include
Drivers/CMSIS/Include
```

and the STM32H755 device header in:

```text
Drivers/CMSIS/Device/ST/STM32H7xx/Include
```

Only the CMSIS headers are used by the current library build.

## Standalone library build

From the repository root:

```bash
cmake -S . -B build/stm32h755 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDAS_DEVICE=nucleo_h755zi_q \
  -DSTM32_CUBE_H7_DIR=/path/to/STM32CubeH7

cmake --build build/stm32h755 --parallel
```

This builds the static library target:

```text
das
```

with the CMake alias:

```text
das::das
```

The archive is normally produced as:

```text
build/stm32h755/libdas.a
```

The library uses `-Wall -Wextra -Werror` during its own compilation.

## Cross-compilation toolchain

The repository provides:

```text
cmake/toolchains/arm-none-eabi.cmake
```

for Cortex-M7 bare-metal builds.

A consuming project may use its own equivalent toolchain file. The important part is that the parent project and DAS are configured in the same cross-compilation context.

Do not configure the parent as a host build and then expect `add_subdirectory()` to turn only DAS into an ARM target. CMake toolchains apply to the build tree, not to whichever directory happens to deserve special treatment that afternoon.

## Including DAS with `add_subdirectory`

A common repository layout is:

```text
my-firmware/
├── CMakeLists.txt
├── src/
└── third_party/
    └── device-abstraction-stack/
```

Set the DAS cache variables before adding the subdirectory:

```cmake
set(DAS_DEVICE nucleo_h755zi_q CACHE STRING "DAS target device" FORCE)
set(STM32_CUBE_H7_DIR "/path/to/STM32CubeH7" CACHE PATH "STM32CubeH7 root" FORCE)

add_subdirectory(third_party/device-abstraction-stack)

target_link_libraries(my_firmware PRIVATE das::das)
```

The application receives the public DAS include path through the target link interface, so source files can simply use:

```c
#include <das/gpio.h>
#include <das/board.h>
```

Do not add `src/mcu/...` or CMSIS backend directories manually to application include paths as a workaround. That bypasses the abstraction boundary.

## Including DAS with `FetchContent`

DAS can also be brought into a CMake build using `FetchContent`:

```cmake
include(FetchContent)

set(DAS_DEVICE nucleo_h755zi_q CACHE STRING "DAS target device" FORCE)
set(STM32_CUBE_H7_DIR "/path/to/STM32CubeH7" CACHE PATH "STM32CubeH7 root" FORCE)

FetchContent_Declare(
    das
    GIT_REPOSITORY https://github.com/Inczert/device-abstraction-stack.git
    GIT_TAG        main
)

FetchContent_MakeAvailable(das)

target_link_libraries(my_firmware PRIVATE das::das)
```

For reproducible products, replace `main` with a release tag or exact commit.

## Installed-package status

DAS does **not yet** provide an `install()`/exported `find_package(DAS)` package configuration.

At the current project stage, supported source integration is through the repository itself, normally `add_subdirectory()` or `FetchContent`.

An install/export interface can be added once the public API and supported-target model have stabilized enough that packaging it would not merely immortalize an early layout mistake.

## What DAS configures internally

For `nucleo_h755zi_q`, the library currently compiles:

```text
src/mcu/stm32h7/gpio.c
src/device/nucleo_h755zi_q/board.c
```

and defines for its own compilation:

```text
CORE_CM7
STM32H755xx
```

These are backend implementation details. Application code should not rely on them being exposed through `das::das`.

## What the consuming firmware must provide

Linking DAS does not create a complete firmware image. The application is responsible for the platform pieces appropriate to its system, including:

- startup/reset code;
- vector table;
- linker script and memory map;
- system clock configuration;
- C/C++ runtime initialization as needed;
- `main` or RTOS entry point;
- NVIC priority and interrupt-vector ownership;
- any bootloader/application split.

This separation is intentional. DAS is a device abstraction library, not a board-generated application skeleton.

## Hardware-test build

To include the repository's physical STM32H755 qualification firmware, enable:

```bash
-DDAS_BUILD_HARDWARE_TESTS=ON
```

The helper script does this for you:

```bash
./scripts/build_stm32h755.sh /path/to/STM32CubeH7 --clean
```

It builds:

```text
build/stm32h755/tests/hardware/stm32h755/das_stm32h755_hw_test.elf
```

The hardware-test executable has its own minimal linker script and reset handler. Those files belong to the test image and are not linked into ordinary applications.

## Running the full physical campaign

```bash
./scripts/stm32h755_test_campaign.sh \
  /path/to/STM32CubeH7 \
  --clean
```

The campaign builds, starts OpenOCD, flashes and checks the target through GDB, asks for required physical wiring/visual confirmation, and packages the evidence into a timestamped archive.

See [Hardware qualification](testing.md).

## Build configuration summary

| CMake variable | Required | Current meaning |
| --- | --- | --- |
| `DAS_DEVICE` | yes | Compile-time board/device selector; currently `nucleo_h755zi_q` |
| `STM32_CUBE_H7_DIR` | yes for current target | Root of STM32CubeH7 used for CMSIS headers |
| `DAS_BUILD_HARDWARE_TESTS` | no | Build the physical STM32H755 qualification image; default `OFF` |
| `CMAKE_TOOLCHAIN_FILE` | required for standalone ARM cross-build | Selects the ARM bare-metal compiler/toolchain |
| `CMAKE_BUILD_TYPE` | no | Standard CMake build type, normally `Debug` during bring-up |
