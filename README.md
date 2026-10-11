# DAS — Device Abstraction Stack

DAS is a small layered C library for embedded systems. It separates application-facing APIs from CPU architecture support, silicon-specific peripheral code, physical board mappings, and final firmware build policy.

The current STM32 path uses **CMSIS definitions directly**. It does not require STM32 HAL/LL, CubeIDE, CubeMX-generated initialization, or generated linker/startup files.

## Current target and status

| Area | Current support |
| --- | --- |
| Language | C11 |
| Build | CMake 3.20+ / `arm-none-eabi-gcc` |
| Architecture | Cortex-M |
| Device | STM32H755 |
| Board | NUCLEO-H755ZI-Q |
| Cores | CM7 and CM4 builds; both physically qualified under debugger control |
| Startup | reusable weak Cortex-M reset/runtime handlers plus canonical STM32H755 vector table |
| Linker | default CM7/CM4 layouts plus custom-linker override |
| Interrupts | opaque DAS IRQ handles; CMSIS NVIC backend; weak per-handler vector ownership |
| Clock/power | 64/200/300/400 MHz stock-board profiles; RCC/PWR/FLASH sequencing |
| Time | monotonic milliseconds, SysTick backend, external/RTOS source injection |
| GPIO | input/output, pulls, output type/speed, AF configuration, EXTI |
| UART | polling/blocking and finite-time I/O; 7/8 application data bits, parity, 1/2 stop bits |
| SPI | modes 0..3, both bit orders, polling and full-duplex DMA transfer |
| I2C | 7-bit controller, 100/400 kHz, probe/read/write/repeated-START |
| Timer/PWM | periodic timer IRQ path plus PWM frequency/duty control |
| DMA/cache | generic DMA1/DMAMUX1 API plus explicit CM7 D-cache coherency |
| Ethernet | CM7 polling Layer-2 MAC/DMA/RMII backend with LAN8742A PHY and raw frame TX/RX; CM4 runtime ownership intentionally unsupported |
| Board API | LEDs, B1, ST-LINK VCP, Arduino UART/I2C/SPI/PWM, D3/D4 and RJ45 Ethernet resources |
| Packaging | static `libdas.a`, install/export, relocatable `find_package(DAS 0.1 CONFIG REQUIRED)` package |
| Qualification | STM32H755 physical regression **40/40 PASS** including CM7/CM4 hard-float startup and CM7 Ethernet Layer 2 |

The repository version is defined once in the root `VERSION` file; the current release candidate is `v0.1.0`. Development on `develop` never creates tags. A release tag is created automatically only after the candidate has been manually hardware-qualified, merged/pushed to `main`, and the normal `main` CI run succeeds.

## Architecture

```text
Application / RTOS
        |
        v
Public DAS API                include/das/
        |
        +------------------+
        |                  |
        v                  v
 common logic          board policy         src/common/, src/board/
                           |
                           v
                      device backend        src/device/stm32h755/
                           |
                           v
                      Cortex-M layer        src/mcu/cortex_m/
                           |
                           v
                          CMSIS
                           |
                           v
                       hardware
```

The public API uses DAS and standard C types. STM32 register types, peripheral instance identifiers, RCC fields, alternate-function numbers and raw IRQ numbers stay below that boundary.

See [Architecture](docs/architecture.md).

## Documentation

- **Architecture and public contract:** [architecture](docs/architecture.md), [API reference](docs/api.md), [porting guidance](docs/porting.md)
- **Boot and target composition:** [build/integration](docs/integration.md), [linker/memory](docs/memory-layout.md), [interrupts/vectors](docs/interrupts.md), [clock/power](docs/clocks.md), [time](docs/time.md)
- **Peripherals:** [board resources](docs/board.md), [UART](docs/uart.md), [SPI](docs/spi.md), [I2C](docs/i2c.md), [timers/PWM](docs/timer.md), [DMA/cache](docs/dma.md), [Ethernet Layer 2](docs/ethernet.md)
- **Integration and acceptance:** [HardRT](docs/rtos-hardrt.md), [hardware/CI testing](docs/testing.md), [release procedure](release/README.md), [release notes](release/v0.1.0.md)

These pages describe the **implemented** v0.1.0 scope. ADC, watchdog, internal flash/reset-cause, production CM7-to-CM4 lifecycle/HSEM/shared-memory coordination, asynchronous Ethernet and the optional C++ wrapper are tracked as later work, not implicitly included.

## Building DAS

CM7 example:

```bash
cmake -S . -B build/cm7 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDAS_DEVICE=nucleo_h755zi_q \
  -DDAS_CORE=cm7 \
  -DSTM32_CUBE_H7_DIR=/path/to/STM32CubeH7

cmake --build build/cm7 --parallel
```

Use a separate build directory with `-DDAS_CORE=cm4` for CPU2. STM32CubeH7 is used for CMSIS core/device headers only; HAL and LL sources are not linked.

## Consuming the generated static library

DAS can be installed as a normal CMake package:

```bash
cmake --install build/cm7 --prefix /path/to/das-install
```

The install contains the generated static archive, public headers, CMake package files and the linker script selected for that package:

```text
lib/libdas.a
include/das/...
lib/cmake/DAS/DASConfig.cmake
lib/cmake/DAS/DASConfigVersion.cmake
lib/cmake/DAS/DASBuildInfo.cmake
lib/cmake/DAS/DASTargets.cmake
share/das/<selected-linker-script>.ld
share/licenses/DAS/LICENSE
```

A separate firmware project consumes it with:

```cmake
find_package(DAS 0.1 CONFIG REQUIRED)

add_executable(my_firmware src/main.c)
target_link_libraries(my_firmware PRIVATE das::das)
```

Application code can include individual public headers or use the convenience umbrella:

```c
#include <das/das.h>
```

Configure the application with the matching ARM core toolchain and install prefix in `CMAKE_PREFIX_PATH`. The imported `das::das` target carries the installed linker script to the final ELF.

DAS is pre-1.0. Installed package compatibility is intentionally constrained to the same minor release line: a `0.1.x` package satisfies `find_package(DAS 0.1 ...)`, but does not silently satisfy a future `0.2` request.

Source-tree `add_subdirectory()` and `FetchContent` integration remain supported as alternatives. See [Building and integration](docs/integration.md).

## Startup and vector-table ownership

DAS supplies weak reusable Cortex-M reset/runtime handlers, the linker-symbol contract and the canonical STM32H755 vector table. The table contains the fixed CMSIS/ST device layout and references weak standard handler symbols for core exceptions and external IRQs.

Normal firmware keeps that DAS-owned table and overrides only the handlers it owns with strong definitions. Applications and RTOSes therefore do **not** need to copy `.isr_vector` merely to provide `SysTick_Handler`, `PendSV_Handler`, `TIM2_IRQHandler`, USART/SPI/I2C/DMA handlers, or similar entries.

Whole-table replacement is deliberately exceptional. Firmware with a custom boot/startup policy may set:

```cmake
set(DAS_USE_DEFAULT_VECTOR_TABLE OFF)
find_package(DAS 0.1 CONFIG REQUIRED)
```

or provide a strong `g_das_vector_table` definition. The default linker scripts still place `.isr_vector` at the correct core image base and provide the runtime symbols consumed by DAS startup.

See [Interrupt model](docs/interrupts.md) and [Building and integration](docs/integration.md).

## Hardware qualification

The standing NUCLEO-H755ZI-Q campaign is **40/40 PASS** at commit `737863075dfed78f73f99ae58f6e0ca52ffea93e`, qualified on 2026-10-04 against STM32CubeH7 commit `f5c0b7a2b1f6eb26fde150f72edb2d7deb647066`.

The campaign covers linker/layout, startup including hardware-FPU enablement and real VFP arithmetic on both cores, clock/time, GPIO/EXTI/IRQ, board resources/button, UART, SPI, I2C, DMA/cache and timer/PWM on both cores where applicable, plus the CM7 Layer-2 Ethernet MAC/DMA/RMII/LAN8742A path.

Ethernet qualification requires JP6 and JP7 fitted and board RJ45 CN14 connected directly to a Linux host Ethernet port. The scripts default to host interface `enp0s31f6`:

```bash
./scripts/stm32h755_test_campaign.sh \
  /path/to/STM32CubeH7 \
  --clean
```

Override only when necessary with `--eth-iface <linux-interface>` or `DAS_ETH_IFACE=<linux-interface>`. No IP address is required; the Ethernet case exchanges raw Layer-2 frames.

The qualified Ethernet run negotiated 100 Mbps/full duplex, validated 5/5 STM32-to-host frames and 64/64 host-to-STM32 integrity frames, with zero integrity errors and `DAS_OK` at completion.

See [Hardware qualification](docs/testing.md) and [Ethernet](docs/ethernet.md).


## Release policy

Release work is deliberately split at the hardware boundary.

Manual work:
- prepare the candidate and version on `develop`;
- require `develop` CI to pass;
- run the complete NUCLEO-H755ZI-Q campaign and require exactly **40 PASS / 0 FAIL / exit 0**;
- merge the qualified `develop -> main` release PR without changing the tested source content; direct pushes do not authorize a tag.

Automatic work:
- normal `main` CI runs;
- after successful `main` CI, the tag workflow reads `VERSION`;
- for a new release, verify the successful `main` commit belongs to the merged `develop -> main` PR; if `v<VERSION>` does not exist, create and push the annotated tag;
- the packaging workflow builds CM7/CM4 Release packages, standalone `.a` libraries and checksums, then publishes the GitHub Release.

A `develop` push never creates a tag. Later `main` pushes with an already-released `VERSION` also do not create another tag. See [release/README.md](release/README.md).
