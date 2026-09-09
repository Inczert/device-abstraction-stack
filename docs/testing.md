# Hardware qualification

DAS treats target testing as part of backend qualification. The STM32H755 campaign combines static linker/image checks with physical execution on **both** Cortex-M cores and packages all evidence into one archive.

Current target:

```text
Board:    NUCLEO-H755ZI-Q
Device:   STM32H755
CPU1:     Cortex-M7
CPU2:     Cortex-M4
Debug:    ST-LINK direct DAP + OpenOCD + GDB
```

The campaign deliberately distinguishes debugger-driven CM4 execution from production dual-core boot coordination. Running CPU2 under GDB proves that the CM4 image, startup path and GPIO/EXTI backend work on the real core. It does not yet prove a final CM7-to-CM4 release/HSEM/shared-memory design.

## Running the campaign

```bash
./scripts/stm32h755_test_campaign.sh \
  /path/to/STM32CubeH7 \
  --clean
```

Example:

```bash
./scripts/stm32h755_test_campaign.sh \
  /home/dev/STM32Cube/Repository/STM32CubeH7/ \
  --clean
```

## Build phase

Before OpenOCD starts, the campaign builds three independent images:

```text
CM7 hardware image
    DAS_CORE=cm7
    default linker: stm32h755_cm7.ld

CM4 hardware image
    DAS_CORE=cm4
    default linker: stm32h755_cm4.ld

CM7 custom-link smoke image
    DAS_CORE=cm7
    DAS_LINKER_SCRIPT=tests/link/stm32h755/custom_cm7.ld
```

The custom-link fixture starts at `0x08020000` and exists solely to prove linker-script override propagation through `das::das`.

## Static memory-layout checks

### CM7 default

Expected placement:

```text
vector table    0x08000000
code/load image flash bank 1
.data/.bss      AXI SRAM
stack top       0x24080000
```

### CM4 default

Expected placement:

```text
vector table    0x08100000
code/load image flash bank 2
.data/.bss      D2 SRAM1
stack top       0x30020000
```

### Custom linker override

Expected placement:

```text
vector table    0x08020000
code/load image remaining flash bank 1 allocation
.data/.bss      AXI SRAM
stack top       0x24080000
```

All three checks require the reusable Cortex-M startup symbols and verify that static/heap usage does not overlap the reserved stack.

## Dual-core OpenOCD

The physical phase uses:

```text
scripts/openocd_h755_dual_core.cfg
```

This uses ST-LINK direct DAP rather than HLA because OpenOCD cannot expose both STM32H755 cores through HLA.

The two GDB servers are:

```text
:3333 -> STM32H755 cpu0 -> Cortex-M7 / CPU1
:3334 -> STM32H755 cpu1 -> Cortex-M4 / CPU2
```

Both cores are probed before either hardware image is programmed.

Expected CPUID part numbers:

```text
CM7 -> 0xC27
CM4 -> 0xC24
```

## Core bring-up and startup checks

Each core independently runs the same hardware-test firmware built for that core.

For both CM7 and CM4 the campaign:

- loads the corresponding ELF;
- runs GDB `compare-sections`;
- starts only the selected target from the dual-core debug session;
- checks firmware boot evidence and heartbeat;
- checks GPIO clocks/modes;
- corrupts one `.data` and one `.bss` object;
- resets/starts the selected core;
- verifies `.data` restoration and `.bss` clearing;
- verifies SCB VTOR equals the linked vector-table address;
- verifies no startup/fault error was recorded.

## GPIO qualification on both cores

The same five automated electrical tests are executed once from CM7 and again from CM4.

### Pull-up

D3 / PE13 is disconnected and configured with the internal pull-up. The physical input must read high.

### Pull-down

The same disconnected input is configured with the internal pull-down. The physical input must read low.

### Loopback low/high

Connect:

```text
CN10 D4 / PE14 / pin 8   ---- jumper ----   CN10 D3 / PE13 / pin 10
       output                                   input / EXTI13
```

The signal must leave one pad, traverse the jumper and be read through the other pad.

### Open-drain

The same loopback validates driven-low and released/high behavior.

### EXTI rising/falling

The output generates physical edges into the input. Both cores must receive and clear rising/falling interrupt events through their own STM32H755 EXTI CPU view.

This is the important dual-core GPIO check: CM7 uses the CPU1 RCC/EXTI view and CM4 uses the CPU2 view. A CM4 compile alone would not prove that distinction.

Do not connect either loopback pin to 3V3, 5V or GND.

## Board LED checks

The visual LED qualification remains on CM7:

- all LEDs off;
- green only;
- yellow only;
- red only;
- all three blinking together.

The board mapping is shared between the two cores, while CM4 GPIO output/input is already exercised physically through the loopback tests. Repeating five human visual confirmations on CM4 would add ceremony rather than coverage.

Board mapping:

| LED | GPIO | Active state |
| --- | --- | --- |
| LD1 green | PB0 | high |
| LD2 yellow | PE1 | high |
| LD3 red | PB14 | high |

## Expected summary

A complete expanded campaign contains **24 acceptance points**:

```text
STM32H755 CM7 memory layout      PASS
STM32H755 CM4 memory layout      PASS
Custom linker override           PASS
CM7 OpenOCD probe                PASS
CM4 OpenOCD probe                PASS
CM7 CMSIS/GPIO bring-up          PASS
CM7 Cortex-M startup/reset       PASS
CM7 GPIO pull-up                 PASS
CM7 GPIO pull-down               PASS
CM7 GPIO loopback low/high       PASS
CM7 GPIO open-drain              PASS
CM7 GPIO EXTI rising/falling     PASS
CM7 LED all off                  PASS
CM7 LED green only               PASS
CM7 LED yellow only              PASS
CM7 LED red only                 PASS
CM7 LED all blink                PASS
CM4 CMSIS/GPIO bring-up          PASS
CM4 Cortex-M startup/reset       PASS
CM4 GPIO pull-up                 PASS
CM4 GPIO pull-down               PASS
CM4 GPIO loopback low/high       PASS
CM4 GPIO open-drain              PASS
CM4 GPIO EXTI rising/falling     PASS
```

The script exits nonzero if any acceptance point fails.

## Wiring sequence

The campaign asks for four fixture states:

1. CM7 pull tests: D3 disconnected;
2. CM7 loopback/EXTI tests: D4 connected to D3;
3. CM4 pull tests: disconnect the jumper again;
4. CM4 loopback/EXTI tests: reconnect D4 to D3.

The repeated disconnect/reconnect is intentional so each core gets the same physical coverage rather than inheriting an assumption from the other core's run.

## Evidence bundle

Every campaign produces one archive:

```text
build/stm32h755/campaign/das-stm32h755-campaign-<UTC timestamp>.tar.gz
```

Typical contents include:

```text
summary.txt
metadata.txt
cm7_build.log
cm4_build.log
custom_link_build.log
openocd-dual-core.log

cm7_memory_layout.log
cm4_memory_layout.log
custom_memory_layout.log

CM7_OpenOCD_probe.log
CM4_OpenOCD_probe.log
CM7_flash_probe.log
CM4_flash_probe.log
CM7_startup_reset.log
CM4_startup_reset.log
CM7_GPIO_*.log
CM4_GPIO_*.log
CM7_LED_*.log

stm32h755_cm7.ld
stm32h755_cm4.ld
custom_cm7.ld

das_stm32h755_cm7_hw_test.elf
das_stm32h755_cm7_hw_test.map
das_stm32h755_cm4_hw_test.elf
das_stm32h755_cm4_hw_test.map
das_stm32h755_custom_link_test.elf
das_stm32h755_custom_link_test.map

cm7-symbols.txt
cm4-symbols.txt
custom-symbols.txt
cm7-elf-size.txt
cm4-elf-size.txt
custom-elf-size.txt
```

The bundle is produced on failure as well, once logging has started.

## Build-only mode

Build either physical image directly:

```bash
./scripts/build_stm32h755.sh /path/to/STM32CubeH7 --core cm7 --clean
./scripts/build_stm32h755.sh /path/to/STM32CubeH7 --core cm4 --build-dir build/stm32h755/cm4-hw --clean
```

## Reusing an existing build

`--no-build` reuses the CM7 hardware image, CM4 hardware image and custom-link image. If any ELF/map is missing, the campaign stops rather than silently reducing coverage.

`--clean` and `--no-build` are mutually exclusive.

## Recovery

Explicit destructive recovery remains separate:

```bash
./scripts/stm32h755_recover.sh
```

Recovery keeps the conservative HLA/single-core OpenOCD path. The normal campaign never performs an implicit mass erase.

## Qualification boundary

After a successful 24-case campaign DAS can claim that both STM32H755 cores physically execute the current startup and GPIO/EXTI paths.

It still cannot claim that a production CM7 application correctly boots/releases CM4, coordinates clock-domain initialization, arbitrates shared memory or uses HSEM correctly. Those remain dual-core system features and are tracked separately.
