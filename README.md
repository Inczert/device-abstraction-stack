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
| Startup | reusable weak Cortex-M reset/runtime and core exception handlers |
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

Configure the application with the matching ARM core toolchain and install prefix in `CMAKE_PREFIX_PATH`. The imported `das::das` target carries the installed linker script to the final ELF.

Source-tree `add_subdirectory()` and `FetchContent` integration remain supported as alternatives. See [Building and integration](docs/integration.md).

## Startup and vector-table ownership

DAS supplies weak reusable Cortex-M reset/runtime handlers and the default linker-symbol contract. The **final firmware image still owns its vector table**. A bare-metal application using the DAS startup path must provide an `.isr_vector` containing at least the initial stack pointer and `Reset_Handler`, plus any core/device handlers it uses such as `SysTick_Handler`.

The default linker scripts place that application-owned table at the correct core image base and provide `__StackTop` plus the `.data`/`.bss` symbols consumed by DAS startup.

## External-consumer LED example

`examples/led_blink` is deliberately a separate CMake project. It does not add the DAS source tree. The helper builds and installs `libdas.a`, resolves it with `find_package(DAS)`, links the application, flashes CM7 with OpenOCD, resets into the new vector table, and asks for physical confirmation of the green LED blink:

```bash
./scripts/build_and_flash_led_blink.sh /path/to/STM32CubeH7
```

This installed-package path has been physically validated on the NUCLEO-H755ZI-Q.

## Hardware qualification

The standing STM32H755 regression is **38/38 PASS** at commit `c4bbc578d32c7b81f2ec5aaf38d637d128ca1942`, qualified on 2026-09-10. It covers linker/layout checks and physical execution of startup, clock/time, GPIO/EXTI, board resources/button, UART, SPI, I2C, DMA/cache and timer/PWM on both cores where applicable.

Run the current campaign with:

```bash
./scripts/stm32h755_test_campaign.sh \
  /home/dev/STM32Cube/Repository/STM32CubeH7/ \
  --clean
```

Every run produces a timestamped evidence archive. The installed-package LED example is a separate application/package smoke test and is not counted as a 39th campaign acceptance point.

See [Hardware qualification](docs/testing.md).

## Documentation

- [Public API reference](docs/api.md)
- [Architecture](docs/architecture.md)
- [Building and integration](docs/integration.md)
- [STM32H755 memory/linker policy](docs/memory-layout.md)
- [Board resources](docs/board.md)
- [Clock control](docs/clocks.md)
- [Monotonic time](docs/time.md)
- [Interrupt model](docs/interrupts.md)
- [UART](docs/uart.md)
- [SPI](docs/spi.md)
- [I2C](docs/i2c.md)
- [Timers and PWM](docs/timer.md)
- [DMA and cache coherency](docs/dma.md)
- [Hardware qualification](docs/testing.md)
- [Porting](docs/porting.md)

## Current boundaries

The qualified baseline does **not** yet include production CM7-to-CM4 boot/release and HSEM/shared-memory coordination, ADC, watchdog, internal-flash/reset-cause services, timer input capture, or a generic asynchronous UART callback/buffering model. Those remain explicit follow-up work rather than being implied by the current hardware qualification.
