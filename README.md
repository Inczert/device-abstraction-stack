# DAS — Device Abstraction Stack

DAS is a small, layered C library for embedded systems. It separates application-facing APIs from CPU architecture code, silicon-specific peripheral code, board wiring, and final firmware build policy.

The current STM32 path uses **CMSIS definitions directly**, without STM32 HAL/LL or CubeIDE/CubeMX-generated initialization files.

## Current status

| Area | Support |
| --- | --- |
| Language | C11 |
| Build | CMake 3.20+ / `arm-none-eabi-gcc` |
| Architecture | Cortex-M |
| Device | STM32H755 |
| Board | NUCLEO-H755ZI-Q |
| Cores | CM7 and CM4 startup/GPIO/EXTI physically qualified under debugger control |
| Startup | reusable weak Cortex-M reset/runtime path |
| Linker | default STM32H755 CM7 and CM4 layouts plus custom override qualification |
| Interrupts | device-agnostic IRQ handles/control; CMSIS NVIC backend on Cortex-M |
| Clock | standard board-frequency API with STM32H755 RCC/PWR/FLASH backend, pending final hardware qualification |
| GPIO | input/output, pulls, push-pull/open-drain, AF configuration, EXTI |
| Board API | green/yellow/red user LEDs |
| Debug/test | dual-core OpenOCD + GDB + packaged evidence campaign |

DAS is still early development. The current project version is `0.1.0`.

## Purpose

The goal is to build practical embedded firmware without requiring a bulky generated vendor project while preserving direct, inspectable control over the hardware.

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
src/mcu/cortex_m/
    Cortex-M architecture
    startup, IRQ controller backend, future SysTick/cache/MPU

src/device/stm32h755/
    STM32H755 silicon/peripherals
    GPIO/EXTI and RCC/PWR/FLASH clock engine now, UART/DMA/etc. later

src/board/nucleo_h755zi_q/
    physical Nucleo resources and board policy
    LEDs and standard clock profiles now, button/VCOM/connectors/etc. later

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

CM4 uses the same command with a separate build directory and:

```text
-DDAS_CORE=cm4
```

DAS requires STM32CubeH7 only for CMSIS core/device headers. HAL and LL sources are not linked.

## Using DAS in firmware

A normal parent project needs only the DAS library target:

```cmake
set(DAS_DEVICE nucleo_h755zi_q CACHE STRING "" FORCE)
set(DAS_CORE cm7 CACHE STRING "" FORCE)
set(STM32_CUBE_H7_DIR "/path/to/STM32CubeH7" CACHE PATH "" FORCE)

add_subdirectory(third_party/device-abstraction-stack)

add_executable(my_firmware src/main.c)
target_link_libraries(my_firmware PRIVATE das::das)
```

There is no separate linker pseudo-library. `libdas.a` has no final physical addresses by itself; the selected linker script is propagated through `das::das` and applies when the final ELF is linked.

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

The board layer owns the physical supply/source policy. The STM32H755 device layer owns RCC, PWR, FLASH, PLL and bus-divider programming. On the stock board the default direct-SMPS power path is limited to the VOS1 operating range, so 480 MHz is not advertised.

See [Clock control](docs/clocks.md).

## Interrupt model

Applications use DAS interrupt handles rather than CMSIS/vendor interrupt numbers:

```c
das_irq_t irq = DAS_IRQ_INVALID;

(void)das_gpio_interrupt_get_irq(pin, &irq);
(void)das_irq_set_priority(irq, 3u);
(void)das_irq_clear_pending(irq);
(void)das_irq_enable(irq);
```

On Cortex-M the backend delegates controller access to CMSIS `NVIC_*` helpers. The application does not need to know that the selected architecture uses NVIC or that an STM32 GPIO source maps to a vendor `IRQn_Type` value.

Peripheral/source state remains separate from controller state. For GPIO, EXTI routing/masking/pending belongs to the GPIO/device backend while `das_irq_*()` controls the CPU interrupt-controller line.

See [Interrupt model](docs/interrupts.md).

## Default linker layouts

DAS provides:

```text
cmake/targets/stm32h755_cm7.ld
cmake/targets/stm32h755_cm4.ld
```

The dual-core-safe defaults are:

```text
CM7
  flash bank 1 : 0x08000000..0x080FFFFF
  AXI SRAM     : 0x24000000..0x2407FFFF

CM4
  flash bank 2 : 0x08100000..0x081FFFFF
  D2 SRAM1     : 0x30000000..0x3001FFFF
```

Override the selected default with:

```bash
-DDAS_LINKER_SCRIPT=/path/to/custom.ld
```

The qualification campaign includes a custom-linker override build that relocates a CM7 test image to `0x08020000`, proving that the override is propagated through `das::das`.

See [STM32H755 memory and linker policy](docs/memory-layout.md).

## Reusable Cortex-M startup

The Cortex-M layer provides a weak reset/runtime path that:

1. restores `.data`;
2. clears `.bss`;
3. sets VTOR;
4. executes the required barriers;
5. calls `main()`.

Applications with a bootloader, RTOS or custom startup can replace the weak symbols. Device-specific external IRQ vectors remain part of the final target image.

## Public APIs

Current public headers:

```text
include/das/result.h
include/das/clock.h
include/das/irq.h
include/das/gpio.h
include/das/board.h
include/das/cortex_m/startup.h
```

See [API reference](docs/api.md).

## Hardware qualification

Run the full STM32H755 campaign:

```bash
./scripts/stm32h755_test_campaign.sh \
    /home/dev/STM32Cube/Repository/STM32CubeH7/ \
    --clean
```

The campaign builds the CM7/CM4 hardware images, the custom-linker smoke image, and a dedicated CM7 clock-profile image. The clock image exercises the public 64/200/300/400 MHz profile path before the established GPIO/IRQ qualification continues.

It uses one direct-DAP OpenOCD session:

```text
GDB :3333 -> STM32H755 Cortex-M7 / CPU1
GDB :3334 -> STM32H755 Cortex-M4 / CPU2
```

Both cores are physically exercised for startup, GPIO pulls, loopback, open-drain and EXTI. The EXTI case also qualifies the public DAS IRQ controller path.

The expanded complete campaign contains **25 acceptance points** and packages both core images, clock/profile evidence, linker-layout evidence, GDB/OpenOCD logs and the final summary into one timestamped `.tar.gz`.

The first three acceptance points are host-only linker/layout checks. They intentionally do not require a connected board; hardware qualification starts with the OpenOCD probes.

Important boundary: CM4 execution is currently debugger-driven. Production CM7-to-CM4 boot/release sequencing, HSEM and shared-memory coordination remain separate dual-core system work.

See [Hardware qualification](docs/testing.md).

## Documentation

- [Architecture](docs/architecture.md)
- [Building and integration](docs/integration.md)
- [Clock control](docs/clocks.md)
- [Interrupt model](docs/interrupts.md)
- [STM32H755 memory/linker policy](docs/memory-layout.md)
- [Public API reference](docs/api.md)
- [Porting DAS](docs/porting.md)
- [Hardware qualification](docs/testing.md)

## License

Apache License 2.0. See [LICENSE](LICENSE).
