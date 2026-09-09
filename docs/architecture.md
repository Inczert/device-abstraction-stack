# Architecture

DAS is a compile-time device abstraction stack. It separates portable application APIs, CPU/core architecture support, silicon-specific device code, board wiring, and final firmware build policy.

## Layers

### Public API

```text
include/das/
```

Application-facing headers use DAS and standard C types. STM32/CMSIS device types must not leak into this layer.

### Common logic

```text
src/common/
```

Hardware-independent shared implementation belongs here.

### MCU/core architecture

```text
src/mcu/<architecture>/
```

Current architecture:

```text
src/mcu/cortex_m/
```

This layer contains architecture-only code, in C and/or assembly:

- reset/runtime mechanics;
- core exception defaults;
- NVIC/SCB helpers;
- SysTick;
- PRIMASK/BASEPRI helpers;
- cache/MPU primitives.

It must not contain STM32 peripherals such as GPIO, RCC, USART, DMA, timers or SPI.

The same Cortex-M startup source is built for both STM32H755 cores. Core selection changes compiler/core definitions, not the conceptual ownership of startup.

### Device layer

```text
src/device/<device>/
```

Current device:

```text
src/device/stm32h755/
```

This layer owns STM32H755 silicon behavior:

- GPIO and EXTI/SYSCFG;
- future RCC/PWR/FLASH;
- USART;
- DMA/DMAMUX;
- timers;
- SPI/I2C;
- ADC/watchdog;
- dual-core silicon control where appropriate.

STM32H755 contains two CPUs. Where registers have CPU-specific views, the device backend selects the correct one from `CORE_CM7` or `CORE_CM4`. GPIO register programming is shared silicon behavior, while per-core EXTI/RCC views remain core-aware.

### Board layer

```text
src/board/<board>/
```

Current board:

```text
src/board/nucleo_h755zi_q/
```

This layer describes physical board wiring and named resources:

```text
green LED   -> PB0 / LD1
yellow LED  -> PE1 / LD2
red LED     -> PB14 / LD3
```

Future resources include the user button, VCOM mapping, connector buses and physical clock-source wiring.

### Build/target policy

Physical memory maps and linker scripts are not CPU instructions and are not peripheral drivers. They live under:

```text
cmake/targets/
```

For STM32H755:

```text
stm32h755_cm7.ld
stm32h755_cm4.ld
```

They define default final-image placement for a selected device/core pair.

## Target composition

The public board target is:

```text
DAS_DEVICE=nucleo_h755zi_q
```

The CPU is selected separately:

```text
DAS_CORE=cm7
```

or:

```text
DAS_CORE=cm4
```

The composition therefore becomes:

```text
application
    |
    v
public DAS API
    |
    +----------------------+
    |                      |
    v                      v
common logic           board mapping
                           |
                           v
                       device layer
                           |
                           v
                     Cortex-M layer
                           |
                           v
                        CMSIS
```

At final link time there is an additional build-policy input:

```text
DAS_DEVICE + DAS_CORE
        |
        v
default linker script
        |
        v
final firmware ELF
```

## Why core selection is separate from device selection

`STM32H755` is one device containing:

```text
CPU1: Cortex-M7
CPU2: Cortex-M4
```

The board does not change when choosing which core image is being built. Therefore `DAS_DEVICE` should not encode `_cm7` or `_cm4` into the board name.

`DAS_CORE` selects CPU/FPU compiler flags, CMSIS core header, `CORE_CM7`/`CORE_CM4`, and the core-specific default linker layout.

## Startup ownership

`src/mcu/cortex_m/startup.c` provides weak reusable Cortex-M reset/runtime behavior:

1. copy `.data`;
2. clear `.bss`;
3. program VTOR;
4. execute barriers;
5. enter `main()`.

The startup code consumes linker symbols but does not know STM32H755 addresses. The final image still owns the vector table, including device-specific external IRQ entries.

## Memory ownership

The default STM32H755 layouts intentionally avoid overlap:

```text
CM7:
  flash bank 1 -> code/vector/load image
  AXI SRAM     -> writable sections/stack

CM4:
  flash bank 2 -> code/vector/load image
  D2 SRAM1     -> writable sections/stack
```

This is a default firmware partition, not a hard architectural restriction. Applications remain free to provide a different linker script with `DAS_LINKER_SCRIPT`.

See [STM32H755 memory and linker policy](memory-layout.md).

## Linker propagation

`das::das` is a static archive. The archive itself is not placed into flash or RAM.

The selected linker script is carried as an `INTERFACE` link option so the normal consumer contract remains:

```cmake
target_link_libraries(my_firmware PRIVATE das::das)
```

There is intentionally no `das::linker` target and no `das_configure_firmware()` helper.

## Interrupt ownership

Interrupt handling spans architecture and device concerns:

```text
Cortex-M NVIC/core control       -> src/mcu/cortex_m/
STM32 EXTI/SYSCFG routing        -> src/device/stm32h755/
application/device vector table  -> final firmware target
```

The device backend must also respect the selected CPU's EXTI/RCC view on a dual-core STM32H755.

## Dual-core qualification architecture

The physical campaign uses a single direct-DAP OpenOCD instance exposing both debug targets:

```text
ST-LINK
   |
   v
OpenOCD direct DAP
   |
   +-- GDB :3333 -> STM32H755 cpu0 -> Cortex-M7
   |
   `-- GDB :3334 -> STM32H755 cpu1 -> Cortex-M4
```

The campaign loads and executes a separately linked image on each core and runs the same startup/GPIO/EXTI electrical tests on both.

This is **debugger-driven core execution**. It validates that the code can physically run on CPU2 and reach the real STM32H755 GPIO/EXTI paths, but it is not the production dual-core boot model.

Production dual-core control remains a separate device/system responsibility:

```text
CM7 configures system/clock policy
        |
        v
CM7 releases or wakes CM4
        |
        v
HSEM/shared-memory coordination
        |
        v
independent application execution
```

Those mechanisms belong to the dedicated dual-core work rather than being hidden inside a GPIO test harness.

## Dependency rules

1. `include/das/` exposes no vendor device types.
2. `src/mcu/` contains no vendor peripheral register implementation.
3. `src/device/` contains no board connector/LED assumptions.
4. `src/board/` does not duplicate register backends.
5. linker/memory policy is not placed in `src/mcu/`.
6. applications do not include implementation files from `src/`.
7. multi-core device code must not silently assume CPU1 when building for CPU2.

## Qualification

Compilation alone is not sufficient evidence.

The expanded campaign combines:

- default CM7 linker-layout validation;
- default CM4 linker-layout validation;
- custom linker-override validation;
- physical CM7 startup/reset and GPIO/EXTI validation;
- physical CM4 startup/reset and GPIO/EXTI validation;
- visible board LED behavior on CM7.

A successful run therefore qualifies the current GPIO/EXTI implementation from both cores while keeping production CM4 boot/release, HSEM and shared-memory coordination as explicit later work.
