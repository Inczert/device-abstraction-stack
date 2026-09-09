# DAS — Device Abstraction Stack

DAS is a small, layered C library for embedded systems that separates application code from CPU/core architecture, device-specific peripherals, and board wiring.

The current STM32 path uses **CMSIS core/device definitions directly rather than STM32 HAL or LL**. The goal is not to bury the hardware behind another giant framework. DAS provides stable public APIs while keeping each implementation layer explicit, inspectable, and suitable for physical qualification.

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
| GPIO | input/output, pulls, push-pull/open-drain, speed, alternate-function configuration, EXTI line configuration |
| Board API | green/yellow/red user LEDs |
| Hardware qualification | OpenOCD + GDB + physical wiring/visual confirmation |

Target selection is compile-time. There is no runtime hardware discovery layer.

## Purpose

DAS is intended to make embedded firmware independent of bulky generated vendor projects while preserving direct control over the hardware.

The project aims for:

- stable application-facing C APIs;
- explicit compile-time target selection;
- CPU/core code separated from vendor device peripherals;
- device peripheral backends using CMSIS register definitions directly;
- board mappings for real physical wiring and named resources;
- no required code generator;
- no mandatory heap, scheduler, or RTOS;
- no hidden ownership of application policy;
- hardware behavior qualified on the real target rather than inferred from successful compilation.

The long-term target for the NUCLEO-H755ZI-Q is a workflow based on CMake, the ARM GNU toolchain, CMSIS, OpenOCD and GDB, without requiring CubeIDE/CubeMX-generated startup, linker, clock, or peripheral-initialization files.

## Layer model

```text
+------------------------------------------------+
| Application / RTOS / mission software         |
+------------------------------------------------+
                       |
                       v
+------------------------------------------------+
| Public DAS API                                 |
| include/das/*.h                                |
+------------------------------------------------+
          |                 |                 |
          v                 v                 v
+----------------+  +----------------+  +----------------+
| Common logic   |  | Board layer    |  | Other APIs     |
| src/common/    |  | src/board/     |  | as added       |
+----------------+  +----------------+  +----------------+
                           |
                           v
                  +--------------------+
                  | Device layer       |
                  | src/device/        |
                  | STM32H755          |
                  | peripherals        |
                  +--------------------+
                           |
                           v
                  +--------------------+
                  | MCU/core layer     |
                  | src/mcu/           |
                  | Cortex-M only      |
                  +--------------------+
                           |
                           v
                  +--------------------+
                  | CMSIS definitions  |
                  +--------------------+
                           |
                           v
                  +--------------------+
                  | Physical hardware  |
                  +--------------------+
```

The three hardware-specific layers have deliberately different responsibilities:

```text
src/mcu/cortex_m/
    CPU/core architecture only
    examples: exceptions, NVIC, SCB, SysTick, cache/MPU, startup primitives

src/device/stm32h755/
    STM32H755 on-chip device/peripheral implementation
    examples: GPIO, RCC, EXTI/SYSCFG, USART, DMA, timers, SPI, I2C

src/board/nucleo_h755zi_q/
    physical board wiring/resources
    examples: LEDs, user button, connector buses, virtual COM mapping, clock-source wiring
```

STM32 peripheral register programming does **not** belong in `src/mcu/`. Cortex-M is the CPU architecture; GPIO and USART are properties of the STM32H755 device.

See [Architecture](docs/architecture.md) for the dependency rules.

## Repository layout

```text
include/das/                     Public C API
src/common/                      Hardware-independent shared implementation
src/mcu/<architecture>/          CPU/core architecture support only
src/device/<device>/             On-chip device/peripheral register backends
src/board/<board>/               Physical board mappings and resources
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

CMake currently composes:

```text
MCU/core backend:  cortex_m
Device backend:    stm32h755
Board backend:     nucleo_h755zi_q
```

The Cortex-M source layer is intentionally minimal at the moment. Startup, NVIC and SysTick work will be added there as core-only support. The already-qualified GPIO implementation lives in the STM32H755 device layer.

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

See [Building and integration](docs/integration.md) for source integration and application responsibilities.

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

For raw GPIO:

```c
const das_gpio_pin_t enable = { DAS_GPIO_PORT_E, 14u };

(void)das_gpio_output_init(enable, false);
(void)das_gpio_write(enable, true);
```

Applications should not include files from `src/mcu/`, `src/device/`, or `src/board/` directly.

## Public APIs

Current public headers:

- [`include/das/result.h`](include/das/result.h) — common result codes;
- [`include/das/gpio.h`](include/das/gpio.h) — generic GPIO configuration, I/O, and EXTI-line support;
- [`include/das/board.h`](include/das/board.h) — named board resources, currently NUCLEO-H755ZI-Q user LEDs.

See [API reference](docs/api.md).

## Hardware qualification

The STM32H755 GPIO/device path is physically qualified on a NUCLEO-H755ZI-Q.

Run:

```bash
./scripts/stm32h755_test_campaign.sh \
  /path/to/STM32CubeH7 \
  --clean
```

The campaign checks:

- ST-LINK/OpenOCD attachment and Cortex-M7 identity;
- flash/program-section integrity;
- firmware startup and fault state;
- GPIO clocks and configuration;
- pull-up and pull-down behavior;
- physical output-to-input loopback;
- open-drain behavior;
- rising/falling EXTI delivery;
- board LED states and visible blinking.

It packages a timestamped evidence directory and `.tar.gz` containing build logs, OpenOCD/GDB output, ELF/map/symbol information, tool versions, and the summary.

See [Hardware qualification](docs/testing.md).

## Documentation

- [Architecture and layer rules](docs/architecture.md)
- [Building and integration](docs/integration.md)
- [Public API reference](docs/api.md)
- [Porting DAS](docs/porting.md)
- [Hardware qualification](docs/testing.md)

## License

Apache License 2.0. See [LICENSE](LICENSE).
