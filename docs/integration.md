# Building and integration

This document describes target composition, source-tree integration, installed static-library consumption, startup ownership, and the current STM32H755 build/debug workflow.

## Requirements

For the current STM32H755 target:

- CMake 3.20 or newer;
- ARM GNU bare-metal toolchain (`arm-none-eabi-gcc`);
- an STM32CubeH7 checkout containing CMSIS core and STM32H755 device headers;
- C11 support.

DAS uses CMSIS definitions only. It does not compile or link STM32 HAL or LL source files.

OpenOCD is required only for hardware flash/debug/qualification workflows.

## Target composition

The current board selector is:

```text
DAS_DEVICE=nucleo_h755zi_q
```

STM32H755 contains two Cortex-M cores, so the core is selected separately:

```text
DAS_CORE=cm7
DAS_CORE=cm4
```

The composition is:

```text
MCU/core architecture = cortex_m
CPU core              = cm7 or cm4
Device                = stm32h755
Board                 = nucleo_h755zi_q
```

`DAS_CORE` selects CPU/FPU compiler flags, CMSIS core definitions, the correct core-specific device view and the default linker script. The default is `cm7`.

Use separate CMake build directories for CM7 and CM4.

## Build the static library

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

The generated product is a static archive. Final flash/RAM placement is applied only when a firmware ELF is linked.

## Installed-package consumption

Install one configured DAS build:

```bash
cmake --install build/cm7 --prefix /path/to/das-install
```

The install includes:

```text
lib/libdas.a
include/das/...
lib/cmake/DAS/DASConfig.cmake
lib/cmake/DAS/DASConfigVersion.cmake
lib/cmake/DAS/DASTargets.cmake
share/das/<selected-linker-script>.ld
```

A separate application then uses only the installed package:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_firmware LANGUAGES C)

find_package(DAS CONFIG REQUIRED)

add_executable(my_firmware src/main.c)
target_link_libraries(my_firmware PRIVATE das::das)
```

Configure that application with the ARM toolchain for the same core and point CMake at the installation:

```bash
cmake -S app -B build/app \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/arm-none-eabi.cmake \
  -DDAS_CORE=cm7 \
  -DCMAKE_PREFIX_PATH=/path/to/das-install
```

The package records its selected DAS device/core and rejects an explicitly conflicting `DAS_DEVICE` or `DAS_CORE`. The imported target exposes the installed public include path and carries the installed linker script to the final ELF. The package is relocatable and does not require a source-tree path after installation.

STM32CubeH7 itself is **not** bundled into the installed package.

## Source-tree integration

Projects that prefer to build DAS as part of their own tree can still use:

```cmake
set(DAS_DEVICE nucleo_h755zi_q CACHE STRING "" FORCE)
set(DAS_CORE cm7 CACHE STRING "" FORCE)
set(STM32_CUBE_H7_DIR "/path/to/STM32CubeH7" CACHE PATH "" FORCE)

add_subdirectory(third_party/device-abstraction-stack)

target_link_libraries(my_firmware PRIVATE das::das)
```

`FetchContent` is equivalent at the target boundary:

```cmake
include(FetchContent)

set(DAS_DEVICE nucleo_h755zi_q CACHE STRING "" FORCE)
set(DAS_CORE cm7 CACHE STRING "" FORCE)
set(STM32_CUBE_H7_DIR "/path/to/STM32CubeH7" CACHE PATH "" FORCE)

FetchContent_Declare(
    das
    GIT_REPOSITORY https://github.com/Inczert/device-abstraction-stack.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(das)

target_link_libraries(my_firmware PRIVATE das::das)
```

Use a release tag or fixed commit for reproducible products.

Application code should include only public headers under `include/das/`. Do not add `src/mcu/`, `src/device/` or `src/board/` to consumer include paths.

## Static library and linker policy

`das::das` is a static library. `libdas.a` itself has no final physical addresses. The linker script applies when the firmware executable is created:

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

For source-tree builds the selected linker script is carried through the build-interface target. For installed-package builds `DASConfig.cmake` attaches the installed, relocatable linker-script path to the imported target.

There is no second linker pseudo-library and no required firmware configuration helper.

## Linker selection and override

Without an override:

```text
DAS_CORE=cm7 -> cmake/targets/stm32h755_cm7.ld
DAS_CORE=cm4 -> cmake/targets/stm32h755_cm4.ld
```

Source-tree applications can override the default with `DAS_LINKER_SCRIPT` before adding DAS. The hardware campaign contains a dedicated custom-link smoke build at `0x08020000` to prove that the override reaches the final executable through `das::das`.

See [STM32H755 memory and linker policy](memory-layout.md).

## Startup and vector table

DAS provides weak reusable Cortex-M reset/runtime and core-exception handlers in:

```text
src/mcu/cortex_m/startup.c
include/das/cortex_m/startup.h
```

The default reset path copies `.data`, clears `.bss`, programs VTOR, executes barriers and calls `main()`.

The **final firmware owns its vector table**. The default linker script reserves/places `.isr_vector`, but it does not generate vector contents. A bare-metal application retaining DAS startup must provide a table containing at least:

```text
entry 0 -> __StackTop
entry 1 -> Reset_Handler
```

and the core/device handlers it uses. For example, a firmware using the default DAS SysTick time source needs a vector entry for `SysTick_Handler`. Device-specific external IRQ entries also belong to the final firmware.

This ownership allows a bootloader, RTOS or application to replace the weak handlers and/or vector policy without modifying DAS.

## External-consumer LED example

`examples/led_blink` is the reference installed-package smoke application. Its CMake project uses `find_package(DAS)` and links the imported static library; it does not add the DAS source tree.

From the DAS repository root:

```bash
./scripts/build_and_flash_led_blink.sh /path/to/STM32CubeH7
```

The script:

1. builds CM7 `libdas.a`;
2. installs it to a local prefix;
3. configures the example against that prefix;
4. verifies CMake resolved that generated installation;
5. builds `das_led_blink.elf`;
6. flashes/verifies it with the qualified direct-DAP OpenOCD configuration;
7. resets into the newly programmed vector table;
8. holds CM4 and runs CM7;
9. asks for physical confirmation of the green LED blink.

This path has been physically validated on the NUCLEO-H755ZI-Q.

## Dual-core hardware qualification

The full campaign builds both core images and starts one direct-DAP OpenOCD instance:

```text
GDB :3333 -> CM7 / CPU1
GDB :3334 -> CM4 / CPU2
```

The campaign proves both independently linked images execute on real CPU1/CPU2 and covers the supported peripheral paths. CM4 execution is still debugger-driven; production CM7-to-CM4 boot/release, HSEM and shared-memory ownership remain separate work under #20.

## CMake variables

| Variable | Default | Meaning |
| --- | --- | --- |
| `DAS_DEVICE` | `nucleo_h755zi_q` | selected board/target |
| `DAS_CORE` | `cm7` | selected CPU core (`cm7` or `cm4`) |
| `DAS_LINKER_SCRIPT` | empty | source-tree custom linker override |
| `STM32_CUBE_H7_DIR` | empty | STM32CubeH7 root used while building DAS |
| `DAS_BUILD_LINK_TESTS` | `OFF` | build linker-smoke target |
| `DAS_BUILD_HARDWARE_TESTS` | `OFF` | build physical qualification firmware |
