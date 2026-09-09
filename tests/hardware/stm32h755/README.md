# STM32H755 qualification target

Target board: **NUCLEO-H755ZI-Q / STM32H755**.

The physical test firmware in this directory executes on **Cortex-M7 / CPU1**. The full campaign also builds a separate **Cortex-M4 / CPU2 linker-smoke image** for static memory-layout qualification.

## What this directory owns

The CM7 hardware-test image provides:

- the STM32H755 external-IRQ vector layout needed by the test;
- test application logic and GDB evidence;
- physical GPIO/LED qualification behavior.

It consumes:

- `das::das` for reusable Cortex-M startup and public GPIO/board APIs;
- the core-selected default STM32H755 linker script propagated by `das::das`;
- CMSIS device definitions for independent register evidence.

There is no test-local linker script and no separate `das::linker` target.

## Linker defaults

CM7 physical image:

```text
DAS_CORE=cm7
vector/code  -> flash bank 1 at 0x08000000
.data/.bss   -> AXI SRAM at 0x24000000
stack top    -> 0x24080000
```

CM4 static link image built by the campaign:

```text
DAS_CORE=cm4
vector/code  -> flash bank 2 at 0x08100000
.data/.bss   -> D2 SRAM1 at 0x30000000
stack top    -> 0x30020000
```

CM4 is not flashed or executed by this campaign. Physical CPU2 boot/release belongs to the dual-core work.

## Qualified paths

The complete campaign checks:

- CM7 static memory/linker layout;
- CM4 static memory/linker layout;
- Cortex-M7/OpenOCD attachment before flashing;
- CM7 ELF programming and `compare-sections`;
- reusable Cortex-M reset/runtime initialization;
- `.data` restoration and `.bss` clearing;
- VTOR/vector placement;
- GPIO clock/mode configuration;
- pull-up and pull-down;
- physical output-to-input loopback;
- open-drain behavior;
- rising/falling EXTI delivery;
- green/yellow/red LED states and synchronized blinking.

A successful complete run currently reports **15 PASS / 0 FAIL**.

## Wiring

For pull tests leave **CN10 D3 / PE13 / pin 10 disconnected**.

For loopback/open-drain/EXTI tests connect one jumper:

```text
CN10 D4 / PE14 / pin 8   ---- jumper ----   CN10 D3 / PE13 / pin 10
       output                                   input / EXTI13
```

Do not connect either pin to 3V3, 5V or GND.

Board LEDs:

- LD1 green: PB0, active high;
- LD2 yellow: PE1, active high;
- LD3 red: PB14, active high.

## Run

```bash
./scripts/stm32h755_test_campaign.sh \
  /path/to/STM32CubeH7 \
  --clean
```

The campaign packages both core linker scripts, both ELF/map pairs, static memory-layout logs, CM7 physical GDB/OpenOCD logs, symbols, tool metadata and the final summary into a timestamped `.tar.gz`.

Build the CM7 physical image only:

```bash
./scripts/build_stm32h755.sh /path/to/STM32CubeH7 --clean
```

Explicit destructive recovery remains separate:

```bash
./scripts/stm32h755_recover.sh
```

See [`docs/testing.md`](../../../docs/testing.md) and [`docs/memory-layout.md`](../../../docs/memory-layout.md) for the complete acceptance and memory-policy contracts.
