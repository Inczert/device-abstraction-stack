# Architecture

DAS is a compile-time device abstraction stack. It separates portable application APIs, CPU/core architecture support, silicon-specific device code, board wiring/policy and final firmware build/link policy.

## Layers

### Public API

```text
include/das/
```

Application-facing headers use DAS and standard C types. STM32 and CMSIS device types do not leak into this layer.

Current public areas are result/error handling, clock, monotonic time, IRQ control, GPIO/EXTI, UART, SPI, I2C, periodic timer/PWM, generic DMA, D-cache maintenance, Layer-2 Ethernet, board resources and explicit Cortex-M startup/vector customization.

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

It does not own STM32 peripheral registers.

### Device layer

```text
src/device/stm32h755/
```

This layer owns STM32H755 silicon behavior implemented by DAS:

- GPIO and EXTI/SYSCFG routing;
- RCC, PWR and FLASH clock/power sequencing;
- UART/USART;
- SPI;
- I2C;
- general-purpose timer/PWM support;
- generic DMA1/DMAMUX1;
- Ethernet MAC + dedicated Ethernet DMA + MDIO/LAN8742A management;
- canonical STM32H755 vector-table layout using CMSIS IRQ numbering;
- core-aware register views where the device exposes CPU-specific state.

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
- RJ45/RMII route to the on-board LAN8742A;
- stock-board clock/power profiles.

Board code knows connector/pin/AF/polarity facts. Device code knows peripheral registers. Application code sees semantic board resources plus generic peripheral handles.

### Build and target policy

```text
cmake/targets/
cmake/DASConfig.cmake.in
```

The build layer owns CM7/CM4 default linker scripts, core/toolchain selection, optional linker override, default-vector-table link policy, static-library installation/export and propagation of the selected linker script to the final firmware ELF.

## Target composition

```text
DAS_DEVICE=nucleo_h755zi_q
DAS_CORE=cm7 | cm4
```

Composition is:

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

`DAS_DEVICE` names the board/target composition. `DAS_CORE` selects CPU/FPU flags, CMSIS core definitions, core-specific STM32 views and default linker layout.

## Startup and vector ownership

`src/mcu/cortex_m/startup.c` provides weak reusable reset/runtime behavior:

1. copy `.data`;
2. clear `.bss`;
3. program VTOR from `__vector_table_start__`;
4. execute architecture barriers;
5. call `main()`.

For STM32H755, `src/device/stm32h755/vector_table.c` is the canonical vector layout. CMSIS/ST supplies IRQ numbering; DAS supplies the table and weak standard handler symbols.

Normal firmware therefore uses:

```text
DAS vector table
    ├── Reset_Handler      -> weak DAS default
    ├── HardFault_Handler  -> weak DAS default / strong app or RTOS override
    ├── PendSV_Handler     -> weak DAS default / strong RTOS override
    ├── SysTick_Handler    -> weak DAS default / strong RTOS override
    ├── TIM2_IRQHandler    -> weak DAS default / strong owner override
    ├── USARTx_IRQHandler  -> weak DAS default / strong owner override
    └── ...
```

A strong handler definition replaces only that handler while the DAS-owned table remains unchanged. Applications do not copy the complete vector table merely to bind an ISR.

`DAS_USE_DEFAULT_VECTOR_TABLE=ON` is the normal source-tree and installed-package policy. CMake force-links `g_das_vector_table` so static archive extraction cannot silently omit the table.

Whole-table replacement is exceptional. A bootloader or specialized runtime may set `DAS_USE_DEFAULT_VECTOR_TABLE=OFF` and supply its own `.isr_vector`, or provide a strong `g_das_vector_table` definition.

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

Applications can replace this with a custom linker script. See [Memory/linker policy](memory-layout.md).

The CM7 AXI-SRAM default is also usable by the Ethernet DMA engine. DTCM must not be used for Ethernet descriptors/frame buffers.

## DMA and cache ownership

Two distinct DMA paths exist:

```text
generic das_dma_t / SPI DMA -> DMA1 + DMAMUX1
Ethernet raw TX/RX          -> ETH peripheral DMA descriptors
```

They are intentionally not conflated. Generic DMA register/DMAMUX configuration belongs to the device layer. Ethernet owns its private descriptor engine. D-cache maintenance belongs to the Cortex-M layer.

Generic caller-owned DMA buffers require explicit caller coherency. The Ethernet backend owns cache maintenance for its internal descriptors/buffers.

## Ethernet boundary

DAS Ethernet stops at Layer 2:

```text
application / optional network stack
        |
        v
DAS Layer-2 API
        |
        v
STM32H755 MAC + ETH DMA
        |
        v
RMII / LAN8742A / RJ45
```

ARP, IP, DHCP, UDP, TCP, DNS and socket semantics belong above DAS. A future lwIP adapter may consume the Layer-2 API without changing public DAS types.

Current runtime ownership is CM7-only. CM4 keeps the API for build/source compatibility but rejects Ethernet initialization before changing board routing.

## Interrupt ownership

```text
Cortex-M NVIC/core control       -> src/mcu/cortex_m/
STM32 peripheral/source state    -> src/device/stm32h755/
board route/polarity             -> src/board/nucleo_h755zi_q/
canonical vector table           -> src/device/stm32h755/vector_table.c
strong handler implementation    -> owning DAS driver / application / RTOS
```

`das_irq_t` represents a controller line. GPIO, timer and generic DMA sources resolve their controller line while retaining source-specific flags/masks in their own APIs.

The current Ethernet baseline is polling-only, so no ETH IRQ ownership contract is introduced yet.

## Dual-core boundary

The hardware campaign uses direct-DAP OpenOCD:

```text
GDB :3333 -> CM7 / CPU1
GDB :3334 -> CM4 / CPU2
```

Independent images execute on both physical CPUs and exercise supported peripheral paths. This validates CPU2 execution and core-aware backends, but it is not production dual-core lifecycle control.

Still separate under #20:

- CM7-to-CM4 boot/release or wake sequencing without debugger assistance;
- deterministic shared clock/system ownership;
- HSEM/inter-core synchronization;
- shared-memory ownership/cache policy.

Ethernet is explicitly CM7-owned in the current baseline.

## Dependency rules

1. `include/das/` exposes no vendor device types.
2. `src/mcu/` contains architecture behavior, not STM32 peripheral drivers.
3. `src/device/` contains silicon behavior, including vector layout and peripheral engines, not NUCLEO connector policy.
4. `src/board/` owns board wiring/policy and reuses generic/device backends.
5. linker/memory/package policy stays in the build layer.
6. applications do not include implementation files from `src/`.
7. multi-core device code must not silently assume CPU1 when built for CPU2.
8. normal firmware keeps the DAS vector table and overrides individual weak handlers; whole-table replacement is explicit.
9. protocol/network stacks above Ethernet do not leak their types into core DAS APIs.

## Qualification

The standing STM32H755 campaign is **39/39 PASS** at DAS commit `f6b65672d9ae69cf28cd574d0dbba01cf875d8dc`, qualified on 2026-09-13 against STM32CubeH7 `f5c0b7a2b1f6eb26fde150f72edb2d7deb647066`.

It includes static linker checks and physical qualification of startup/vector ownership, clock/power, time, board resources/button, GPIO/EXTI/IRQ, UART, SPI, I2C, generic DMA/cache and timer/PWM on both cores where applicable, plus CM7 polling Layer-2 Ethernet MAC/DMA/RMII/LAN8742A raw TX/RX.
