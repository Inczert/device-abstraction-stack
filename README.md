# DAS — Device Abstraction Stack

DAS is a small, layered C library for embedded systems. It separates application-facing APIs from CPU architecture code, silicon-specific peripheral code, physical board mappings, and final firmware build policy.

The current STM32 path uses **CMSIS definitions directly**, without STM32 HAL/LL or CubeIDE/CubeMX-generated initialization files.

## Current status

| Area | Support |
| --- | --- |
| Language | C11 |
| Build | CMake 3.20+ / `arm-none-eabi-gcc` |
| Architecture | Cortex-M |
| Device | STM32H755 |
| Board | NUCLEO-H755ZI-Q |
| Cores | CM7 and CM4 startup/GPIO/EXTI/time physically qualified under debugger control |
| Startup | reusable weak Cortex-M reset/runtime path |
| Linker | default STM32H755 CM7 and CM4 layouts plus custom override qualification |
| Interrupts | device-agnostic IRQ handles/control; CMSIS NVIC backend on Cortex-M |
| Clock | 64/200/300/400 MHz board profiles with STM32H755 RCC/PWR/FLASH backend, hardware-qualified |
| Time | generic monotonic millisecond API; CMSIS SysTick backend plus external/RTOS source injection |
| GPIO | input/output, pulls, push-pull/open-drain, AF configuration, EXTI |
| UART | polling/timeout UART physically qualified on CM7 and CM4 |
| Timer/PWM | periodic timer IRQ and PWM physically qualified on CM7 and CM4 |
| SPI | controller path physically qualified on CM7 and CM4 across modes 0..3 |
| I2C | controller path physically qualified on CM7 and CM4 at 100/400 kHz |
| DMA/cache | memory and SPI DMA physically qualified on CM7/CM4; explicit CM7 D-cache maintenance |
| Board API | LEDs, B1 user button, ST-LINK VCP, Arduino/Zio UART/I2C/SPI and D3/D4 fixture mappings |
| Debug/test | dual-core OpenOCD + GDB + packaged 38-case evidence campaign |

DAS is still early development. The current project version is `0.1.0`.

## Purpose

The goal is to build practical embedded firmware without requiring a generated vendor project while preserving direct, inspectable control over the hardware.

DAS aims for:

- stable public C APIs;
- compile-time target selection;
- explicit CPU/core selection on multi-core devices;
- Cortex-M code separated from STM32 peripheral code;
- board wiring separated from device registers;
- CMSIS retained as a low-level implementation dependency rather than exposed as the application contract;
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
src/common/
    hardware-independent semantics
    monotonic time helpers

src/mcu/cortex_m/
    Cortex-M architecture
    startup, IRQ controller, SysTick backend

src/device/stm32h755/
    STM32H755 silicon/peripherals
    GPIO/EXTI, RCC/PWR/FLASH, UART, timer/PWM, SPI, I2C and DMA

src/board/nucleo_h755zi_q/
    physical NUCLEO resources and board policy
    LEDs, B1, connector groups and standard clock profiles

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

The selected core controls CPU/FPU compiler flags, CMSIS core definitions, core-specific device views, and the default linker script. The default core is `cm7`.

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

CM4 uses a separate build directory and `-DDAS_CORE=cm4`.

DAS requires STM32CubeH7 only for CMSIS core/device headers. HAL and LL sources are not linked.

## Consuming the generated static library

DAS can be installed as a CMake package. The install contains the generated `libdas.a`, public headers, package targets and the selected core linker script:

```bash
cmake --install build/cm7 --prefix /path/to/das-install
```

A separate firmware project can then consume only that generated package:

```cmake
find_package(DAS CONFIG REQUIRED)

add_executable(my_firmware src/main.c)
target_link_libraries(my_firmware PRIVATE das::das)
```

Configure the application with the matching ARM toolchain/core and the install prefix in `CMAKE_PREFIX_PATH`. The imported `das::das` target carries the installed linker script to the final firmware ELF.

`examples/led_blink` deliberately exercises this external-consumer path rather than using `add_subdirectory()`. From the DAS repository root:

```bash
./scripts/build_and_flash_led_blink.sh /path/to/STM32CubeH7
```

The helper builds and installs the CM7 static library, configures the example against that installation with `find_package(DAS)`, builds the standalone firmware, flashes it with OpenOCD and asks for visual confirmation of the green LED blink.

Source-tree embedding with `add_subdirectory()` remains supported for projects that prefer to build DAS as part of their own CMake tree.

## Clock model

Applications request a standard board frequency in hertz instead of calculating PLL dividers:

```c
#include <das/clock.h>

if (das_clock_frequency_supported(400000000u)) {
    (void)das_clock_set_frequency(400000000u);
}
```

The NUCLEO-H755ZI-Q backend currently advertises:

```text
64 MHz
200 MHz
300 MHz
400 MHz
```

The board layer owns physical supply/source policy. The STM32H755 device layer owns RCC, PWR, FLASH, PLL and bus-divider programming. On the stock board 480 MHz is deliberately not advertised for the qualified direct-SMPS/VOS1 profile.

See [Clock control](docs/clocks.md).
