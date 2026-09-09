# DAS — Device Abstraction Stack

DAS is a small, layered C library for embedded systems that separates application code from CPU/core architecture, device-specific peripherals, and board wiring.

The current STM32 path uses **CMSIS core/device definitions directly rather than STM32 HAL or LL**. The goal is not to replace one giant vendor framework with another. DAS provides stable public APIs while keeping each implementation layer explicit, inspectable, and suitable for physical qualification.

## Status

DAS is in early development. The current CMake project version is `0.1.0`.

| Area | Current support |
| --- | --- |
| Language | C11 |
| Build system | CMake 3.20+ |
| CPU/core architecture | Cortex-M, current target Cortex-M7 |
| Device | STM32H755 |
| Board | NUCLEO-H755ZI-Q |
| Vendor dependency | STM32CubeH7 CMSIS headers only |
| Cortex-M startup | reusable reset/runtime initialization and weak core exception defaults |
| GPIO | input/output, pulls, push-pull/open-drain, speed, alternate-function configuration, EXTI line configuration |
| Board API | green/yellow/red user LEDs |
| Hardware qualification | OpenOCD + GDB + physical wiring/visual confirmation |

Target selection is compile-time. There is no runtime hardware-discovery layer.

## Purpose

DAS is intended to make embedded firmware independent of bulky generated vendor projects while preserving direct control over the hardware.

The project aims for:

- stable application-facing C APIs;
- explicit compile-time target selection;
- CPU/core code separated from vendor device peripherals;
- device peripheral backends using CMSIS register definitions directly;
- board mappings for real physical wiring and named resources;
- optional reusable startup/core support without forcing application policy;
- no required code generator;
- no mandatory heap, scheduler, or RTOS;
- hardware behavior qualified on the real target rather than inferred from successful compilation.

The long-term target for the NUCLEO-H755ZI-Q is a workflow based on CMake, the ARM GNU toolchain, CMSIS, OpenOCD and GDB, without requiring CubeIDE/CubeMX-generated startup, linker, clock, or peripheral-initialization files.

## Layer model

```text
Application / RTOS / mission software
                |
                v
         Public DAS API
        include/das/*.h
                |
        +-------+-------+
        |               |
        v               v
   Common logic     Board layer
   src/common/      src/board/
                        |
                        v
                  Device layer
                  src/device/
                        |
                        v
                  MCU/core layer
                  src/mcu/
                        |
                        v
                 CMSIS definitions
                        |
                        v
                 Physical hardware
```

The hardware-specific layers have deliberately different responsibilities:

```text
src/mcu/cortex_m/
    CPU/core architecture only
    current: startup/reset/runtime and weak core exception handlers
    future: NVIC, SysTick, cache/MPU helpers

src/device/stm32h755/
    STM32H755 on-chip device/peripheral implementation
    current: GPIO + EXTI/SYSCFG routing
    future: RCC/PWR/FLASH, USART, DMA, timers, SPI, I2C, ADC...

src/board/nucleo_h755zi_q/
    physical board wiring/resources
    current: LEDs
    future: user button, connector buses, VCOM mapping, board clock sources...
```

STM32 peripheral register programming does **not** belong in `src/mcu/`. Cortex-M is the CPU architecture; GPIO, RCC and USART are properties of the STM32H755 device.

See [Architecture](docs/architecture.md) for the dependency rules.

## Repository layout

```text
include/das/                     Public C API
src/common/                      Hardware-independent shared implementation
src/mcu/<architecture>/          CPU/core architecture support only
src/device/<device>/             On-chip device/peripheral backends
src/board/<board>/               Physical board mappings/resources
cmake/toolchains/                Cross-compilation toolchains
examples/                        User-facing examples as APIs mature
tests/hardware/                  Physical-target qualification firmware
scripts/                         Build, OpenOCD, GDB and campaign helpers
docs/                            Project documentation
```

## Current target composition

For:

```cmake
-DAS_DEVICE=nucleo_h755zi_q
```

CMake composes:

```text
MCU/core backend:  cortex_m
Device backend:    stm32h755
Board backend:     nucleo_h755zi_q
```

The current library sources include:

```text
src/mcu/cortex_m/startup.c
src/device/stm32h755/gpio.c
src/board/nucleo_h755zi_q/board.c
```

## Building for NUCLEO-H755ZI-Q

DAS currently expects an STM32CubeH7 checkout for the CMSIS core and STM32H755 device headers. It does **not** compile or link STM32 HAL or LL source files.

```bash
cmake -S . -B build/stm32h755 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
  -DDAS_DEVICE=nucleo_h755zi_q \
  -DSTM32_CUBE_H7_DIR=/path/to/STM32CubeH7

cmake --build build/stm32h755 --parallel
```

The library target is:

```cmake
das::das
```

and the build produces `libdas.a`.

See [Building and integration](docs/integration.md) for source integration, the startup linker contract, and application responsibilities.

## Using DAS from an application

A parent CMake project can include DAS directly:

```cmake
set(DAS_DEVICE nucleo_h755zi_q CACHE STRING "" FORCE)
set(STM32_CUBE_H7_DIR "/path/to/STM32CubeH7" CACHE PATH "" FORCE)

add_subdirectory(third_party/device-abstraction-stack)

target_link_libraries(my_firmware PRIVATE das::das)
```

Application code should include public headers only:

```c
#include <das/board.h>
#include <das/gpio.h>

void application_init(void)
{
    (void)das_board_led_init(DAS_BOARD_LED_GREEN, false);
}

void application_tick(void)
{
    (void)das_board_led_toggle(DAS_BOARD_LED_GREEN);
}
```

Applications should not include implementation files from `src/mcu/`, `src/device/`, or `src/board/` directly.

## Reusable Cortex-M startup

The Cortex-M layer currently provides an optional weak `Reset_Handler` and weak default core exception handlers.

The reset path:

1. copies `.data` from its load address into RAM;
2. clears `.bss`;
3. programs SCB VTOR from the linker-provided vector-table symbol;
4. executes DSB/ISB barriers;
5. calls the application's `main()`.

A bootloader, RTOS, or application can provide strong replacements for the weak startup/exception symbols. Device-specific external interrupt vectors remain the responsibility of the selected target image rather than the generic Cortex-M layer.

The startup path requires linker symbols documented in [Building and integration](docs/integration.md). Reusable STM32H755 linker support is tracked separately.

## Public APIs

Current public headers include:

- [`include/das/result.h`](include/das/result.h) — common result codes;
- [`include/das/gpio.h`](include/das/gpio.h) — GPIO configuration, I/O, and EXTI support;
- [`include/das/board.h`](include/das/board.h) — named board resources;
- [`include/das/cortex_m/startup.h`](include/das/cortex_m/startup.h) — optional Cortex-M startup/core exception entry points.

See [API reference](docs/api.md) for peripheral semantics.

## Hardware qualification

The STM32H755/Cortex-M7 path is physically qualified on a NUCLEO-H755ZI-Q.

```bash
./scripts/stm32h755_test_campaign.sh \
  /path/to/STM32CubeH7 \
  --clean
```

The current campaign checks:

- ST-LINK/OpenOCD attachment and Cortex-M7 identity;
- flash/program-section integrity;
- reusable Cortex-M reset/runtime behavior;
- `.data` restoration and `.bss` clearing after reset;
- VTOR/vector-table placement;
- firmware heartbeat and fault state;
- GPIO clocks/configuration;
- pull-up/pull-down behavior;
- physical output-to-input loopback;
- open-drain behavior;
- rising/falling EXTI delivery;
- board LED states and visible blinking.

A complete current campaign contains **13 acceptance points** and packages a timestamped `.tar.gz` evidence bundle.

See [Hardware qualification](docs/testing.md).

## Documentation

- [Architecture and layer rules](docs/architecture.md)
- [Building and integration](docs/integration.md)
- [Public API reference](docs/api.md)
- [Porting DAS](docs/porting.md)
- [Hardware qualification](docs/testing.md)

## License

Apache License 2.0. See [LICENSE](LICENSE).
