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
| Startup | reusable weak Cortex-M reset/runtime handlers plus default STM32H755 vector table |
| Linker | default CM7/CM4 layouts plus custom-linker override |
| Interrupts | opaque DAS IRQ handles; CMSIS NVIC backend; GPIO/timer/DMA IRQ resolution |
| Clock/power | 64/200/300/400 MHz stock-board profiles; RCC/PWR/FLASH sequencing |
| Time | monotonic milliseconds, SysTick backend, external/RTOS source injection |
| GPIO | input/output, pulls, output type/speed, AF configuration, EXTI |
| UART | polling/blocking and finite-time I/O; 7/8 application data bits, parity, 1/2 stop bits |
| SPI | modes 0..3, both bit orders, polling and full-duplex DMA transfer |
| I2C | 7-bit controller, 100/400 kHz, probe/read/write/repeated-START |
| Timer/PWM | periodic timer IRQ path plus PWM frequency/duty control |
| DMA/cache | generic DMA API, STM32H755 DMA1/DMAMUX1, explicit CM7 D-cache coherency |
| Board API | LEDs, B1, ST-LINK VCP, Arduino UART/I2C/SPI/PWM and D3/D4 resources |
| Packaging | static `libdas.a`, install/export, relocatable `find_package(DAS CONFIG REQUIRED)` package |
| Qualification | packaged dual-core OpenOCD/GDB hardware campaign, **38/38 PASS** |

DAS is still early development. The project version is currently `0.1.0`.

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
lib/cmake/DAS/DASTargets.cmake
share/das/<selected-linker-script>.ld
```

A separate firmware project consumes it with:

```cmake
find_package(DAS CONFIG REQUIRED)

add_executable(my_firmware src/main.c)
target_link_libraries(my_firmware PRIVATE das::das)
```

Application code can include individual public headers or use the convenience umbrella:

```c
#include <das/das.h>
```

The umbrella exposes the normal application-facing API. Architecture-specific startup/vector customization remains explicit through headers such as `das/cortex_m/startup.h`.

Configure the application with the matching ARM core toolchain and install prefix in `CMAKE_PREFIX_PATH`. The imported `das::das` target carries the installed linker script to the final ELF.

Source-tree `add_subdirectory()` and `FetchContent` integration remain supported as alternatives. See [Building and integration](docs/integration.md).

## Startup and vector-table ownership

DAS supplies weak reusable Cortex-M reset/runtime handlers, the linker-symbol contract and a default STM32H755 vector table. The device table uses the CMSIS STM32H755 IRQ numbering internally, provides the core exception/SysTick entries required for a simple bare-metal application, and routes unused external IRQ slots to weak default handlers.

Normal applications therefore do **not** need to write an `.isr_vector` merely to boot. `DAS_USE_DEFAULT_VECTOR_TABLE` is `ON` by default for source-tree and installed-package consumers.

Firmware that owns its vector/ISR policy can set:

```cmake
set(DAS_USE_DEFAULT_VECTOR_TABLE OFF)
find_package(DAS CONFIG REQUIRED)
```

or provide a strong `g_das_vector_table` definition, which overrides the weak DAS default. The default linker scripts still place `.isr_vector` at the correct core image base and provide `__StackTop` plus the `.data`/`.bss` symbols consumed by DAS startup.

## External-consumer LED example

`examples/led_blink` is deliberately a separate CMake project. It does not add the DAS source tree and contains no application vector table or local startup/halt boilerplate. The helper builds and installs `libdas.a`, resolves it with `find_package(DAS)`, links the application, flashes CM7 with OpenOCD, resets into the new image, and asks for physical confirmation of the green LED blink:

```bash
./scripts/build_and_flash_led_blink.sh /path/to/STM32CubeH7
```

## Hardware qualification

The standing STM32H755 regression is **38/38 PASS** at commit `fd9246abf76278556962724684b308264d37049f`, qualified after the centralized vector-table refactor. It covers linker/layout checks and physical execution of startup, clock/time, GPIO/EXTI, board resources/button, UART, SPI, I2C, DMA/cache and timer/PWM on both cores where applicable.
