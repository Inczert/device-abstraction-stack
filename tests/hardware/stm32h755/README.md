# STM32H755 physical qualification target

Target: **NUCLEO-H755ZI-Q / STM32H755 Cortex-M7**.

This directory contains the minimal freestanding firmware used by the DAS physical hardware campaign. It is a qualification image, not an application template.

The image intentionally uses:

- the public DAS API for GPIO/board control;
- CMSIS core/device definitions through the DAS STM32H7 backend;
- no STM32 HAL or LL;
- a test-local reset handler and linker script so it can be flashed independently;
- an evidence structure inspected by GDB for deterministic acceptance checks.

## Qualified paths

The current campaign validates:

- Cortex-M7/OpenOCD attachment before flashing;
- ELF programming and `compare-sections` integrity;
- startup, heartbeat, and HardFault/error state;
- GPIO clock/mode configuration;
- internal pull-up and pull-down;
- physical output-to-input loopback;
- open-drain driven-low/released behavior;
- rising/falling EXTI delivery through a physical jumper;
- green, yellow, and red board LED states and synchronized blinking.

## Wiring

For pull tests, leave **CN10 D3 / PE13 / pin 10 disconnected**.

For loopback/open-drain/EXTI tests, connect one jumper:

```text
CN10 D4 / PE14 / pin 8   ---- jumper ----   CN10 D3 / PE13 / pin 10
       output                                   input / EXTI13
```

Do not connect either test pin to 3V3, 5V, or GND.

Board LED mapping:

- LD1 green: PB0, active high;
- LD2 yellow: PE1, active high;
- LD3 red: PB14, active high.

## Run

From the repository root:

```bash
./scripts/stm32h755_test_campaign.sh /path/to/STM32CubeH7 --clean
```

The campaign creates a timestamped evidence directory and a `.tar.gz` bundle containing build output, OpenOCD/GDB logs, ELF/map/symbol information, tool versions, and the summary. The archive is also produced for failed campaigns when logging has already started.

Build only:

```bash
./scripts/build_stm32h755.sh /path/to/STM32CubeH7 --clean
```

Explicit destructive recovery:

```bash
./scripts/stm32h755_recover.sh
```

Recovery is intentionally separate from qualification so a failed test never triggers an implicit mass erase.

For the complete acceptance contract and evidence format, see [`docs/testing.md`](../../../docs/testing.md).
