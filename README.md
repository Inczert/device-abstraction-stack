# DAS — Device Abstraction Stack

DAS is a small, layered C library for embedded systems that separates application code from MCU registers, board wiring, and vendor-specific implementation details.

The project is deliberately built around **CMSIS core/device definitions rather than STM32 HAL or LL**. The goal is not to hide the hardware behind a giant framework. The goal is to provide a stable, explicit API while keeping the backend close enough to the registers that behavior remains understandable, deterministic, and testable.

## Status

DAS is in early development. The current CMake project version is `0.1.0`.

| Area | Current support |
| --- | --- |
| Language | C11 |
| Build system | CMake 3.20+ |
| MCU family | STM32H7 |
| Validated MCU | STM32H755, Cortex-M7 core |
| Validated board | NUCLEO-H755ZI-Q |
| Vendor dependency | STM32CubeH7 CMSIS headers only |
| GPIO | input/output, pulls, push-pull/open-drain, speed, alternate-function configuration, EXTI line configuration |
| Board API | green/yellow/red user LEDs |
| Hardware qualification | OpenOCD + GDB + physical wiring/visual confirmation |

More MCU families, boards, and peripherals are intended to be added behind the same public API model. There is currently no runtime target discovery: **the target device is selected at build time**.

## Why DAS exists

Typical embedded projects eventually accumulate one of two problems: application code depends directly on a vendor HAL everywhere, or a home-grown abstraction grows until it becomes another vendor HAL wearing a different hat.

DAS aims for a narrower model:

- stable application-facing C APIs;
- compile-time selection of the target device;
- MCU backends that use CMSIS register definitions directly;
- board/device layers that describe real wiring and named resources;
- no required code generator;
- no hidden heap or scheduler requirement;
- startup code, clock policy, RTOS choice, and application architecture remain owned by the application;
- hardware behavior is qualified on the real target, not merely inferred from successful compilation.

## Layer model

```text
+---------------------------------------------+
| Application / RTOS / mission software      |
+---------------------------------------------+
                    |
                    v
+---------------------------------------------+
| Public DAS API                             |
| include/das/*.h                            |
| GPIO, board resources, result types        |
+---------------------------------------------+
             |                    |
             v                    v
+-----------------------+   +------------------+
| Device / board layer  |   | Common logic     |
| src/device/<board>/   |   | src/common/      |
| semantic -> pins      |   | shared behavior  |
+-----------------------+   +------------------+
             |
             v
+---------------------------------------------+
| MCU-family backend                          |
| src/mcu/<family>/                           |
| register-level implementation               |
+---------------------------------------------+
                    |
                    v
+---------------------------------------------+
| CMSIS core + device headers                 |
+---------------------------------------------+
                    |
                    v
+---------------------------------------------+
| MCU registers / physical hardware           |
+---------------------------------------------+
```

The application should normally include only headers from `include/das/`. MCU-specific headers stay inside the backend. A board layer may translate semantic resources such as `DAS_BOARD_LED_GREEN` into a physical GPIO such as `PB0`.

See [Architecture](docs/architecture.md) for dependency rules and the intended growth model.

## Repository layout

```text
include/das/                     Public C API
src/common/                      MCU-independent implementation shared by backends
src/mcu/<family>/                MCU-family register backends
src/device/<board>/              Board/device mappings and semantic resources
cmake/toolchains/                Cross-compilation toolchains
examples/                        User-facing examples as APIs mature
tests/hardware/                  Physical-target qualification firmware
scripts/                         Build, OpenOCD, GDB and test-campaign helpers
docs/                            Project documentation
```

Some directories are intentionally reserved for future layers and may still be empty while the project is being built out.

## Building for the NUCLEO-H755ZI-Q

DAS currently expects an STM32CubeH7 checkout because the STM32H7 backend uses the CMSIS core and STM32H755 device headers from it. It does **not** link STM32 HAL or LL source files.

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

and the resulting static library is `libdas.a`.

The current target selection is compile-time only:

```cmake
-DAS_DEVICE=nucleo_h755zi_q
```

For this device DAS also defines the backend compilation for `CORE_CM7` and `STM32H755xx` internally.

See [Building and integration](docs/integration.md) for standalone builds, `add_subdirectory`, `FetchContent`, and application responsibilities.

## Using DAS from an application

A parent CMake project can include DAS directly:

```cmake
set(DAS_DEVICE nucleo_h755zi_q CACHE STRING "" FORCE)
set(STM32_CUBE_H7_DIR "/path/to/STM32CubeH7" CACHE PATH "" FORCE)

add_subdirectory(third_party/device-abstraction-stack)

target_link_libraries(my_firmware PRIVATE das::das)
```

Application code then uses public headers only:

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

For a raw GPIO:

```c
const das_gpio_pin_t enable = { DAS_GPIO_PORT_E, 14u };

(void)das_gpio_output_init(enable, false);
(void)das_gpio_write(enable, true);
```

DAS does not provide your reset handler, linker script, system clock setup, scheduler, or RTOS. Those belong to the firmware using the library. The hardware qualification image under `tests/hardware/` has its own minimal startup only because it must be independently flashable for testing.

## Public APIs

The currently implemented public headers are:

- [`include/das/result.h`](include/das/result.h) — common result codes;
- [`include/das/gpio.h`](include/das/gpio.h) — generic GPIO configuration, I/O, and EXTI-line support;
- [`include/das/board.h`](include/das/board.h) — board-level named resources, currently NUCLEO-H755ZI-Q user LEDs.

See [API reference](docs/api.md) for semantics, examples, and interrupt ownership rules.

## Hardware qualification

The STM32H755 backend is tested on a physical NUCLEO-H755ZI-Q using the same conservative OpenOCD configuration used by the companion embedded projects.

Run the full campaign with:

```bash
./scripts/stm32h755_test_campaign.sh \
  /path/to/STM32CubeH7 \
  --clean
```

The campaign checks:

- ST-LINK/OpenOCD attachment and Cortex-M7 identity;
- flash/program-section integrity;
- firmware startup and fault state;
- GPIO clock and mode configuration;
- pull-up and pull-down behavior;
- physical output-to-input loopback;
- open-drain behavior;
- rising/falling EXTI delivery;
- board LED output states and visible blinking.

It produces a timestamped evidence directory and a `.tar.gz` archive containing build output, OpenOCD/GDB logs, ELF/map/symbol information, tool versions, and the final summary.

See [Hardware qualification](docs/testing.md) for wiring and acceptance details.

## Documentation

- [Architecture and layer rules](docs/architecture.md)
- [Building and integration](docs/integration.md)
- [Public API reference](docs/api.md)
- [Adding a new MCU or board](docs/porting.md)
- [Hardware qualification](docs/testing.md)

## License

Apache License 2.0. See [LICENSE](LICENSE).
