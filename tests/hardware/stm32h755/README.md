# STM32H755 physical qualification target

Target: **NUCLEO-H755ZI-Q / STM32H755 Cortex-M7**.

This directory contains the freestanding firmware used by the DAS physical hardware campaign. It is a qualification image, not an application template.

The image uses:

- public DAS APIs for GPIO/board control;
- the reusable DAS Cortex-M reset/runtime path;
- the reusable STM32H755 CM7 linker target `das::linker`;
- CMSIS device definitions for independent STM32H755 evidence;
- no STM32 HAL or LL;
- a test-local STM32H755 vector table;
- an evidence structure inspected by GDB.

The former test-local `linker.ld` has been removed. The qualification image now consumes the same default linker script documented for applications:

```text
cmake/targets/stm32h755_cm7.ld
```

## Qualified paths

The campaign validates:

- ELF/map memory placement before board access;
- Cortex-M7/OpenOCD attachment;
- flash programming and `compare-sections`;
- reusable Cortex-M startup, `.data` restoration and `.bss` clearing;
- VTOR/vector placement;
- GPIO clocks/modes;
- pull-up/down;
- physical loopback;
- open-drain behavior;
- rising/falling EXTI delivery;
- green/yellow/red LED states and blinking.

## Wiring

For pull tests leave **CN10 D3 / PE13 / pin 10 disconnected**.

For loopback/open-drain/EXTI connect one jumper:

```text
CN10 D4 / PE14 / pin 8   ---- jumper ----   CN10 D3 / PE13 / pin 10
       output                                   input / EXTI13
```

Do not connect either test pin to 3V3, 5V or GND.

## Run

```bash
./scripts/stm32h755_test_campaign.sh /path/to/STM32CubeH7 --clean
```

The campaign packages logs, ELF, map, symbol table, exact linker script, tool metadata, and final summary into a timestamped `.tar.gz` archive.

Build only:

```bash
./scripts/build_stm32h755.sh /path/to/STM32CubeH7 --clean
```

Explicit destructive recovery:

```bash
./scripts/stm32h755_recover.sh
```

See [`docs/testing.md`](../../../docs/testing.md) and [`docs/memory-layout.md`](../../../docs/memory-layout.md).
