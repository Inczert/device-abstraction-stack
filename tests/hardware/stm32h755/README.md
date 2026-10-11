# STM32H755 qualification target

Target board: **NUCLEO-H755ZI-Q / STM32H755**.

The hardware-test firmware in this directory is built twice by the full campaign:

```text
DAS_CORE=cm7 -> Cortex-M7 / CPU1 image
DAS_CORE=cm4 -> Cortex-M4 / CPU2 image
```

Both images use the same test source and DAS public APIs. The different CMake build directories provide core-specific compiler flags, CMSIS definitions and linker layouts.

## What this directory owns

Each hardware-test image provides:

- test application logic and GDB evidence;
- physical GPIO qualification behavior;
- startup `.data`/`.bss` probes;
- dedicated hard-float startup execution evidence on both cores.

It consumes:

- the canonical STM32H755 vector table and weak default handlers supplied by DAS;
- `das::das` for reusable Cortex-M startup and GPIO/board APIs;
- the selected STM32H755 linker script propagated by `das::das`;
- CMSIS device definitions for independent register evidence.

There is no test-local linker script and no separate `das::linker` target.

## Core layouts

CM7:

```text
DAS_CORE=cm7
vector/code  -> flash bank 1 at 0x08000000
.data/.bss   -> AXI SRAM at 0x24000000
stack top    -> 0x24080000
```

CM4:

```text
DAS_CORE=cm4
vector/code  -> flash bank 2 at 0x08100000
.data/.bss   -> D2 SRAM1 at 0x30000000
stack top    -> 0x30020000
```

The full campaign also builds a separate custom-link smoke image from `tests/link/stm32h755/` to validate `DAS_LINKER_SCRIPT` override behavior.

## Dual-core debug path

The campaign starts:

```text
scripts/openocd_h755_dual_core.cfg
```

which uses ST-LINK direct DAP and exposes:

```text
GDB :3333 -> CM7 / CPU1
GDB :3334 -> CM4 / CPU2
```

The older conservative HLA configuration remains for explicit recovery; HLA is not suitable for OpenOCD dual-core debugging.

## Qualified paths

The complete campaign checks:

- CM7 default memory/linker layout;
- CM4 default memory/linker layout;
- custom linker override propagation;
- CM7 and CM4 OpenOCD/CPUID attachment before flashing;
- ELF programming and `compare-sections` for both core images;
- reusable Cortex-M reset/runtime initialization on both cores;
- CP10/CP11 enablement plus real VFP arithmetic on both cores;
- `.data` restoration and `.bss` clearing on both cores;
- VTOR/vector placement on both cores;
- GPIO clock/mode configuration from both cores;
- pull-up and pull-down from both cores;
- physical output-to-input loopback from both cores;
- open-drain behavior from both cores;
- rising/falling EXTI delivery through CPU1 and CPU2 views;
- green/yellow/red LED states and synchronized blinking on CM7.

The repository-wide standing campaign now contains **40 acceptance points**; the hard-float qualifier contributes one combined CM7+CM4 acceptance point.

The CM4 execution here is debugger-driven. It proves that CPU2 can run the current DAS startup/GPIO/EXTI code, but it does not yet prove production CM7-to-CM4 boot/release, HSEM or shared-memory coordination.

## Hard-float focused qualifier

No signal wiring is required beyond ST-LINK USB:

```bash
./scripts/stm32h755_fpu_test.sh /path/to/STM32CubeH7
```

The script builds CM7 and CM4 images, rejects an ELF that lacks real VFP single-precision arithmetic, then flashes and checks each physical core through GDB. It requires CP10/CP11 full access, exact result bits for 3.875, no NOCP evidence and a live post-calculation heartbeat.

## Wiring

For each core's pull tests leave **CN10 D3 / PE13 / pin 10 disconnected**.

For each core's loopback/open-drain/EXTI tests connect one jumper:

```text
CN10 D4 / PE14 / pin 8   ---- jumper ----   CN10 D3 / PE13 / pin 10
       output                                   input / EXTI13
```

Do not connect either pin to 3V3, 5V or GND.

The full campaign starts with D3 and D4 disconnected so **both** cores can complete their input pull tests. It then prompts **once** to connect D4 to D3. The jumper stays connected for the CM4 and CM7 loopback/open-drain/EXTI tests and the timer/PWM tests. No repeated disconnect/reconnect is required.

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

The campaign packages both hardware ELF/map pairs, the custom-link ELF/map pair, all linker scripts, static memory checks, per-core GDB logs, dual-core OpenOCD log, symbol tables, tool metadata and the final summary into one timestamped `.tar.gz`.

Build one core image only:

```bash
./scripts/build_stm32h755.sh /path/to/STM32CubeH7 --core cm7 --clean
./scripts/build_stm32h755.sh /path/to/STM32CubeH7 --core cm4 --build-dir build/stm32h755/cm4-hw --clean
```

Explicit destructive recovery remains separate:

```bash
./scripts/stm32h755_recover.sh
```

See [`docs/testing.md`](../../../docs/testing.md) and [`docs/memory-layout.md`](../../../docs/memory-layout.md) for the complete acceptance and memory-policy contracts.
