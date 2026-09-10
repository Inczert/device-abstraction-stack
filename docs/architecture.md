# Architecture

DAS is a compile-time device abstraction stack. It separates portable application APIs, CPU/core architecture support, silicon-specific device code, board wiring/policy and final firmware build/link policy.

## Layers

### Public API

```text
include/das/
```

Application-facing headers use DAS and standard C types. STM32 and CMSIS device types do not leak into this layer.

Current public areas are result/error handling, clock, monotonic time, IRQ control, GPIO/EXTI, UART, SPI, I2C, periodic timer/PWM, DMA, D-cache maintenance, board resources and optional Cortex-M startup/vector override symbols.

### Common logic

```text
src/common/
```

Hardware-independent shared semantics live here, currently including generic monotonic-time helpers.

### MCU/core architecture

```text
src/mcu/cortex_m/
```

This layer owns Cortex-M architecture behavior:

- weak reset/runtime mechanics;
- core exception defaults;
- CMSIS NVIC-backed IRQ controller operations;
- SysTick time backend;
- D-cache primitives/range maintenance.

It does not own STM32 GPIO, RCC, DMA, UART, SPI, I2C or timer registers.

### Device layer

```text
src/device/stm32h755/
```

This layer owns STM32H755 silicon behavior currently implemented by DAS:

- GPIO and EXTI/SYSCFG routing;
- RCC, PWR and FLASH clock/power sequencing;
- UART/USART;
- SPI;
- I2C;
- general-purpose timer/PWM support;
- DMA1/DMAMUX1;
- default STM32H755 vector-table layout using CMSIS IRQ numbering;
- core-aware register views where STM32H755 exposes CPU-specific state.

ADC, watchdog, internal-flash/reset-cause services and production dual-core lifecycle control remain separate follow-up work.

### Board layer

```text
src/board/nucleo_h755zi_q/
```

The board layer owns physical NUCLEO-H755ZI-Q policy and named resources:

- LD1/LD2/LD3 semantic LEDs;
- B1 USER button polarity/routing;
- ST-LINK VCP and Arduino UART routes;
- Arduino I2C route;
- Arduino SPI route and default CS GPIO;
- Arduino D4 PWM route;
- D3/D4 qualification GPIO aliases;
- stock-board clock/power profiles.

Board code knows connector/pin/AF/polarity facts. Device code knows peripheral registers. Application code sees semantic board resources plus generic peripheral handles.

### Build and target policy

```text
cmake/targets/
cmake/DASConfig.cmake.in
```

The build layer owns:

- CM7/CM4 default memory/linker scripts;
- core/toolchain selection;
- optional linker override;
- default-vector-table link policy;
- static-library installation/export;
- relocatable installed `das::das` CMake target;
- propagation of the selected linker script to the final firmware ELF.

## Target composition

```text
DAS_DEVICE=nucleo_h755zi_q
DAS_CORE=cm7 | cm4
```

Composition becomes:

```text
application
    |
    v
public DAS API
    |
    +----------------------+
    |                      |
    v                      v
common logic           board mapping/policy
                           |
                           v
                       STM32H755 device
                           |
                           v
                       Cortex-M layer
                           |
                           v
                          CMSIS
```

`DAS_DEVICE` names the board/target composition. `DAS_CORE` separately selects CPU/FPU flags, CMSIS core definitions, core-specific STM32 views and default linker layout.

## Startup and vector ownership

`src/mcu/cortex_m/startup.c` provides weak reusable reset/runtime behavior:

1. copy `.data`;
2. clear `.bss`;
3. program VTOR from `__vector_table_start__`;
4. execute architecture barriers;
5. call `main()`.

The startup source consumes linker symbols but contains no STM32H755 physical addresses.

For the STM32H755 composition, DAS also places a weak default vector table in the device layer. CMSIS supplies the IRQ numbering; DAS owns the actual default table so a normal bare-metal application does not have to duplicate startup boilerplate merely to boot or use the default SysTick time source.

The default table provides the initial stack, weak Cortex-M handlers and SysTick entries. External STM32H755 IRQ slots safely route to `Default_Handler`; applications that need concrete external ISR bindings currently provide their own table.

The default is force-linked from the otherwise lazy static archive when `DAS_USE_DEFAULT_VECTOR_TABLE=ON`. Firmware that owns vector policy can either:

- set `DAS_USE_DEFAULT_VECTOR_TABLE=OFF` before adding/finding DAS and provide its own `.isr_vector`; or
- provide a strong `g_das_vector_table`, which overrides the weak DAS definition.

This leaves simple applications simple while preserving full bootloader/RTOS/application ownership when needed.

## Memory ownership

The default non-overlapping STM32H755 partition is:

```text
CM7:
  flash bank 1 -> vector/code/load image, base 0x08000000
  AXI SRAM     -> writable sections/stack, top 0x24080000

CM4:
  flash bank 2 -> vector/code/load image, base 0x08100000
  D2 SRAM1     -> writable sections/stack, top 0x30020000
```

Applications can replace this policy with a custom linker script. See [Memory/linker policy](memory-layout.md).

## Linker and package propagation

`das::das` is a static archive target. `libdas.a` is not assigned physical addresses when created.

For a source-tree build, the selected linker script is carried on the target's build interface. For an installed package, `DASConfig.cmake` attaches the installed relocatable linker-script path to the imported target. When the default vector table is enabled, CMake also force-links the canonical weak vector symbol so that static-library extraction cannot silently omit the boot table.

In both cases the consumer contract remains:

```cmake
target_link_libraries(my_firmware PRIVATE das::das)
```

The installed package also exports only the public include tree, keeping implementation/source paths private.

## Interrupt ownership

```text
Cortex-M NVIC/core control       -> src/mcu/cortex_m/
STM32 peripheral/source state    -> src/device/stm32h755/
board route/polarity             -> src/board/nucleo_h755zi_q/
default vector table             -> src/device/stm32h755/vector_table.c
custom vector/ISR binding        -> application/RTOS when required
```

`das_irq_t` represents a controller line. GPIO, timer and DMA sources can resolve their controller line while keeping source-specific flags/masks in their own APIs.

## DMA and cache ownership

STM32H755 DMA register/DMAMUX configuration belongs to the device layer. D-cache maintenance belongs to the Cortex-M layer. Application/driver code owns buffer coherency policy and therefore calls the cache API explicitly around DMA-visible cached memory.

This avoids pretending a generic DMA call can infer cache ownership for arbitrary caller buffers.

## Dual-core boundary

The hardware campaign uses direct-DAP OpenOCD to expose both cores:

```text
GDB :3333 -> CM7 / CPU1
GDB :3334 -> CM4 / CPU2
```

Independent images execute on both physical CPUs and exercise the currently supported peripheral paths. This validates CPU2 execution and core-aware backends, but it is not the production dual-core lifecycle.

Still separate under #20:

- CM7-to-CM4 boot/release or wake sequencing without debugger assistance;
- deterministic shared clock/system ownership;
- HSEM/inter-core synchronization;
- shared-memory ownership/cache policy.

## Dependency rules

1. `include/das/` exposes no vendor device types.
2. `src/mcu/` contains architecture behavior, not STM32 peripheral drivers.
3. `src/device/` contains silicon behavior, including the device vector layout, not NUCLEO connector policy.
4. `src/board/` owns board wiring/policy and reuses generic/device backends.
5. linker/memory/package policy stays in the build layer.
6. applications do not include implementation files from `src/`.
7. multi-core device code must not silently assume CPU1 when built for CPU2.
8. DAS supplies a safe default vector table, while applications/RTOSes can replace it explicitly when they own ISR binding.

## Qualification

The completed standing STM32H755 campaign is **38/38 PASS** at `c4bbc578d32c7b81f2ec5aaf38d637d128ca1942`. It includes static linker checks and physical qualification of startup, clock/power, time, board resources/button, GPIO/EXTI/IRQ, UART, SPI, I2C, DMA/cache and timer/PWM on both cores where applicable.

The installed `find_package(DAS)` CM7 LED blink application is separately hardware-validated on the packaging/example line before this default-vector refactor. The next LED smoke run should qualify the simpler application against the library-owned vector path.
