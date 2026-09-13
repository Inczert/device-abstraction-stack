# Building and integration

This document describes target composition, source-tree integration, installed static-library consumption, startup/vector ownership and the current STM32H755 build/debug workflow.

## Requirements

For the current STM32H755 target:

- CMake 3.20 or newer;
- ARM GNU bare-metal toolchain (`arm-none-eabi-gcc`);
- an STM32CubeH7 checkout containing CMSIS core and STM32H755 device headers;
- C11 support.

DAS uses CMSIS definitions only. It does not compile or link STM32 HAL or LL source files. OpenOCD is required only for hardware flash/debug/qualification workflows.

## Target composition

```text
DAS_DEVICE=nucleo_h755zi_q
DAS_CORE=cm7 | cm4

MCU/core architecture = cortex_m
Device                = stm32h755
Board                 = nucleo_h755zi_q
```

`DAS_CORE` selects CPU/FPU compiler flags, CMSIS core definitions, the correct core-specific device view and the default linker script. Use separate CMake build directories for CM7 and CM4.

## Build the static library

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

Install one configured build:

```bash
cmake --install build/cm7 --prefix /path/to/das-install
```

The package contains:

```text
lib/libdas.a
include/das/...
lib/cmake/DAS/DASConfig.cmake
lib/cmake/DAS/DASConfigVersion.cmake
lib/cmake/DAS/DASTargets.cmake
share/das/<selected-linker-script>.ld
```

A consumer uses:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_firmware LANGUAGES C)

find_package(DAS CONFIG REQUIRED)

add_executable(my_firmware src/main.c)
target_link_libraries(my_firmware PRIVATE das::das)
```

Configure the application with the ARM toolchain for the same core and point `CMAKE_PREFIX_PATH` at the installation. The package records its selected DAS device/core and rejects an explicitly conflicting `DAS_DEVICE` or `DAS_CORE`.

The imported target exposes the installed public include path and carries the installed linker script to the final ELF. STM32CubeH7 itself is not bundled into the package.

## Source-tree integration

```cmake
set(DAS_DEVICE nucleo_h755zi_q CACHE STRING "" FORCE)
set(DAS_CORE cm7 CACHE STRING "" FORCE)
set(STM32_CUBE_H7_DIR "/path/to/STM32CubeH7" CACHE PATH "" FORCE)

add_subdirectory(third_party/device-abstraction-stack)
target_link_libraries(my_firmware PRIVATE das::das)
```

`FetchContent` may be used at the same target boundary. Use a release tag or fixed commit for reproducible products.

Application code should include only public headers under `include/das/`. Do not add `src/mcu/`, `src/device/` or `src/board/` to consumer include paths.

## Static library and linker policy

`das::das` is a static library. The linker script applies when the final executable is created:

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

Default selection:

```text
DAS_CORE=cm7 -> cmake/targets/stm32h755_cm7.ld
DAS_CORE=cm4 -> cmake/targets/stm32h755_cm4.ld
```

Source-tree applications can replace the default with `DAS_LINKER_SCRIPT`. The standing campaign contains a dedicated custom-link fixture at `0x08020000` proving that the override reaches the final executable through `das::das`.

See [STM32H755 memory and linker policy](memory-layout.md).

## Startup and canonical vector table

DAS provides reusable Cortex-M reset/runtime mechanics in:

```text
src/mcu/cortex_m/startup.c
include/das/cortex_m/startup.h
```

For STM32H755, DAS also provides the canonical weak vector-table object:

```text
src/device/stm32h755/vector_table.c
```

CMSIS supplies the authoritative device IRQ numbering. The DAS table contains the initial stack pointer, reset/core exceptions, SysTick and every external STM32H755 vector slot.

External slots reference standard handler symbols such as `TIM2_IRQHandler`, USART/SPI/I2C/DMA/EXTI handlers, etc. DAS supplies weak defaults for those symbols. Normal firmware binds an interrupt by defining only the handler it owns:

```c
void TIM2_IRQHandler(void)
{
    /* strong application/driver definition */
}
```

No application-owned vector table is required for ordinary ISR binding.

Because `libdas.a` is static, `DAS_USE_DEFAULT_VECTOR_TABLE=ON` force-links the canonical weak symbol `g_das_vector_table`. This is the default for source-tree and installed-package consumers.

### Complete vector replacement

Only firmware with a deliberately custom startup/vector policy should replace the table.

Option 1:

```cmake
set(DAS_USE_DEFAULT_VECTOR_TABLE OFF)
find_package(DAS CONFIG REQUIRED)
```

Then provide the application's own `.isr_vector`.

Option 2: leave the default enabled and provide a strong `g_das_vector_table` definition. The strong symbol replaces the weak DAS table.

The dedicated custom-vector CI consumer covers both replacement contracts.

## RTOS integration

An RTOS may retain the DAS vector table and provide strong core handlers. The HardRT 0.5.1 reference integration replaces `HardFault_Handler`, `PendSV_Handler` and `SysTick_Handler` while retaining the DAS-owned vector layout.

Because both libraries are static archives, the reference application explicitly requests those strong HardRT handlers at link time:

```cmake
target_link_options(app PRIVATE
    -Wl,-u,HardFault_Handler
    -Wl,-u,PendSV_Handler
    -Wl,-u,SysTick_Handler)
```

See [HardRT integration](rtos-hardrt.md).

## Ethernet integration boundary

DAS provides raw Layer-2 Ethernet through `<das/eth.h>` and the `DAS_BOARD_ETH_RJ45` semantic route. The current STM32H755 runtime implementation is CM7-owned and polling-only.

Core DAS does not provide ARP, IP, UDP/TCP, DHCP, DNS or sockets. A future network stack such as lwIP should sit above the Layer-2 API without exposing lwIP types from DAS public headers.

The installed `examples/eth_raw` consumer and `scripts/stm32h755_eth_test.sh` qualify package installation plus the real RMII/LAN8742A/MAC/DMA data path.

## Standalone examples

Every directory under `examples/` is a standalone installed-package consumer. CI builds all of them against an installed DAS package on both CM7 and CM4 build configurations where compilation is supported.

Physical helpers include:

```text
scripts/build_and_flash_led_blink.sh
scripts/build_and_flash_hardrt_uart.sh
scripts/build_and_flash_eth_raw.sh
scripts/stm32h755_eth_test.sh
```

The Ethernet physical qualifier defaults to host interface `enp0s31f6`; override with `--iface` or `DAS_ETH_IFACE` when required.

## Dual-core hardware qualification

The campaign exposes:

```text
GDB :3333 -> CM7 / CPU1
GDB :3334 -> CM4 / CPU2
```

It proves independently linked images execute on real CPU1/CPU2 and exercises supported peripheral paths. CM4 execution is still debugger-driven; production CM7-to-CM4 boot/release, HSEM and shared-memory ownership remain separate work under #20.

Ethernet is currently a CM7-only runtime resource.

## CMake variables

| Variable | Default | Meaning |
| --- | --- | --- |
| `DAS_DEVICE` | `nucleo_h755zi_q` | selected board/target |
| `DAS_CORE` | `cm7` | selected CPU core (`cm7` or `cm4`) |
| `DAS_LINKER_SCRIPT` | empty | source-tree custom linker override |
| `DAS_USE_DEFAULT_VECTOR_TABLE` | `ON` | force-link the canonical DAS vector table |
| `STM32_CUBE_H7_DIR` | empty | STM32CubeH7 root used while building DAS |
| `DAS_BUILD_LINK_TESTS` | `OFF` | build linker-smoke target |
| `DAS_BUILD_HARDWARE_TESTS` | `OFF` | build physical qualification firmware |

## Qualified baseline

The standing hardware campaign is **39/39 PASS** at DAS commit `f6b65672d9ae69cf28cd574d0dbba01cf875d8dc`, qualified on 2026-09-13. See [Hardware qualification](testing.md).
