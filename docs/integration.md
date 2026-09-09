# Building and integration

This document describes how DAS selects a target, how it chooses the CPU core on a multi-core device, and how another firmware project consumes the library.

## Requirements

For the current STM32H755 target:

- CMake 3.20 or newer;
- ARM GNU bare-metal toolchain (`arm-none-eabi-gcc`);
- an STM32CubeH7 checkout containing CMSIS core and STM32H755 device headers;
- C11 support.

DAS uses CMSIS definitions only. It does not compile or link STM32 HAL or LL source files.

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

`DAS_CORE` selects:

- ARM compiler/FPU flags;
- CMSIS `core_cm7.h` or `core_cm4.h`;
- `CORE_CM7` or `CORE_CM4`;
- the default STM32H755 linker script.

The default remains `cm7`.

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

CM4:

```bash
cmake -S . -B build/cm4 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDAS_DEVICE=nucleo_h755zi_q \
  -DDAS_CORE=cm4 \
  -DSTM32_CUBE_H7_DIR=/path/to/STM32CubeH7

cmake --build build/cm4 --parallel
```

Use separate build directories for the two cores. Compiler flags, CMSIS core definitions and link policy are different and should not be swapped in-place inside one CMake cache.

Configure output reports the selected composition and linker script.

## Consuming DAS

With `add_subdirectory()`:

```cmake
set(DAS_DEVICE nucleo_h755zi_q CACHE STRING "" FORCE)
set(DAS_CORE cm7 CACHE STRING "" FORCE)
set(STM32_CUBE_H7_DIR "/path/to/STM32CubeH7" CACHE PATH "" FORCE)

add_subdirectory(third_party/device-abstraction-stack)

add_executable(my_firmware
    src/main.c
)

target_link_libraries(my_firmware PRIVATE das::das)
```

That final line is the normal integration contract.

Application code includes public headers:

```c
#include <das/board.h>
#include <das/gpio.h>
```

Do not add `src/mcu/`, `src/device/`, or `src/board/` to application include paths.

## Static library versus linker script

`das::das` is a static library. The archive itself is not linked to a physical address.

The selected `.ld` file is used later, when the final executable is created:

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

DAS propagates the selected linker script from `das::das` through CMake `INTERFACE` link options. Therefore the consumer does **not** link a second pseudo-library and does not call a configuration helper:

```cmake
target_link_libraries(my_firmware PRIVATE das::das)
```

is sufficient for the default bare-metal layout.

## Linker selection

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

A relative override is resolved from the DAS source directory. For product integration, using an absolute path or parent-project CMake path is clearer.

Custom linker scripts remain appropriate for bootloaders, flash partitions, TCM placement, shared-memory layouts, external RAM, RTOS-specific sections, or any other memory policy that differs from the DAS default.

See [STM32H755 memory and linker policy](memory-layout.md).

## Reusable Cortex-M startup

DAS currently includes optional weak Cortex-M startup definitions:

```text
src/mcu/cortex_m/startup.c
include/das/cortex_m/startup.h
```

The default reset path:

1. restores `.data`;
2. clears `.bss`;
3. writes SCB VTOR;
4. executes DSB/ISB;
5. calls `main()`.

Both default STM32H755 linker scripts provide the required symbols:

```text
__data_load__
__data_start__
__data_end__
__bss_start__
__bss_end__
__vector_table_start__
```

A bootloader, RTOS or application may replace the weak startup handlers and/or the linker script.

The generic Cortex-M layer does not define STM32 external-IRQ vectors.

## Core-specific STM32 device definitions

For the selected core DAS privately defines:

```text
STM32H755xx + CORE_CM7
```

or:

```text
STM32H755xx + CORE_CM4
```

The STM32H755 device backend uses the correct core-specific RCC/EXTI view where the silicon has per-core registers.

These definitions are implementation details and should not be treated as public application API.

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

## Hardware qualification

The physical campaign currently executes the CM7 image. It also builds and statically checks a CM4 link-smoke image before flashing the board:

```bash
./scripts/stm32h755_test_campaign.sh \
    /path/to/STM32CubeH7 \
    --clean
```

CM4 physical boot/release is tracked as dual-core work rather than being treated as a linker test.

## CMake variables

| Variable | Default | Meaning |
| --- | --- | --- |
| `DAS_DEVICE` | `nucleo_h755zi_q` | selected board/target |
| `DAS_CORE` | `cm7` | selected CPU core (`cm7` or `cm4`) |
| `DAS_LINKER_SCRIPT` | empty | custom linker override; empty selects device/core default |
| `STM32_CUBE_H7_DIR` | empty | STM32CubeH7 root for CMSIS headers |
| `DAS_BUILD_LINK_TESTS` | `OFF` | build static linker-smoke target |
| `DAS_BUILD_HARDWARE_TESTS` | `OFF` | build CM7 physical qualification target |

## Installed-package status

DAS does not yet provide an installed `find_package(DAS)` package. Source integration currently uses `add_subdirectory()` or `FetchContent`.
