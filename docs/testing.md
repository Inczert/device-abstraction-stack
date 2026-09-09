# Hardware qualification

DAS treats target testing as part of backend qualification. The STM32H755 campaign now separates **static image/layout qualification** from **physical execution qualification** so the CM4 linker work does not pretend that CPU2 has already been booted on hardware.

Current target:

```text
Board:      NUCLEO-H755ZI-Q
Device:     STM32H755
Physical:   Cortex-M7 / CPU1
Static:     Cortex-M7 + Cortex-M4 image/link layouts
Debug:      ST-LINK + OpenOCD + GDB
```

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

The campaign builds two independent images before touching the board:

```text
CM7 hardware image
    DAS_CORE=cm7
    default linker: stm32h755_cm7.ld

CM4 linker-smoke image
    DAS_CORE=cm4
    default linker: stm32h755_cm4.ld
```

The CM4 image is **not flashed or executed** by this campaign. Physical CM4 boot/release, HSEM and shared-memory qualification belong to the dual-core work.

## 1. STM32H755 CM7 memory layout

The actual CM7 hardware-test ELF and map are checked before OpenOCD starts.

Expected default placement:

```text
vector table    0x08000000
code/load image flash bank 1
.data/.bss      AXI SRAM
stack top       0x24080000
```

The checker also requires the Cortex-M startup symbols and verifies heap/static data do not overlap the reserved stack.

## 2. STM32H755 CM4 memory layout

A separate CM4 ELF is cross-compiled and linked using `DAS_CORE=cm4`.

Expected default placement:

```text
vector table    0x08100000
code/load image flash bank 2
.data/.bss      D2 SRAM1
stack top       0x30020000
```

This statically qualifies:

- Cortex-M4/FPU compiler selection;
- `CORE_CM4` CMSIS device view;
- CM4 linker script selection through `das::das`;
- startup symbol contract;
- non-overlapping default CM4 flash/RAM placement.

It does **not** prove CPU2 reset/release or runtime execution.

## 3. Board/OpenOCD probe

Before flashing, GDB attaches through OpenOCD and checks the physical CPU1 core identity.

Expected Cortex-M part number:

```text
0xC27 -> Cortex-M7
```

This step is non-destructive. If attachment fails, the campaign stops before programming a new image.

## 4. Flash and firmware bring-up

The campaign:

- loads the CM7 ELF;
- runs GDB `compare-sections`;
- resets/runs the target;
- checks that firmware booted;
- checks the heartbeat advances;
- checks no fault/error evidence was recorded;
- checks expected GPIO clocks/modes.

## 5. Cortex-M startup/reset

GDB deliberately changes variables in `.data` and `.bss`, resets CPU1, then verifies:

- `.data` is restored from flash;
- `.bss` is zeroed;
- SCB VTOR equals `__vector_table_start__`;
- `main()` is reached again;
- the heartbeat advances;
- no startup/fault error is recorded.

The same reusable startup source is buildable for CM4, but CM4 runtime execution is not claimed here.

## 6-10. GPIO qualification

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

The output generates physical edges into the input. The campaign requires real rising and falling interrupt delivery.

For the CM7 physical image the STM32H755 backend uses the CPU1 RCC/EXTI register view. The same backend is compiled for the CPU2 view in the CM4 static build.

Do not connect either loopback pin to 3V3, 5V or GND.

## 11-15. Board LEDs

The physical CM7 image checks:

- all LEDs off;
- green only;
- yellow only;
- red only;
- all three blinking together.

Static states combine software/register evidence with visual confirmation. Blinking combines an advancing firmware counter with visual confirmation.

Board mapping:

| LED | GPIO | Active state |
| --- | --- | --- |
| LD1 green | PB0 | high |
| LD2 yellow | PE1 | high |
| LD3 red | PB14 | high |

## Expected summary

A complete campaign contains **15 acceptance points**:

```text
STM32H755 CM7 memory layout     PASS
STM32H755 CM4 memory layout     PASS
Board/OpenOCD probe             PASS
CMSIS/GPIO bring-up             PASS
Cortex-M startup/reset          PASS
GPIO pull-up                    PASS
GPIO pull-down                  PASS
GPIO loopback low/high          PASS
GPIO open-drain                 PASS
GPIO EXTI rising/falling        PASS
LED all off                     PASS
LED green only                  PASS
LED yellow only                 PASS
LED red only                    PASS
LED all blink                   PASS
```

The script exits nonzero if any acceptance point fails.

## Evidence bundle

Every campaign creates a timestamped directory under:

```text
build/stm32h755/campaign/
```

and packages it as:

```text
das-stm32h755-campaign-<UTC timestamp>.tar.gz
```

The bundle includes, when available:

```text
summary.txt
metadata.txt
build.log
cm4_link_build.log
openocd.log

cm7_memory_layout.log
cm4_memory_layout.log
board_probe.log
flash_probe.log
cortex_m_startup_reset.log
GPIO_*.log
LED_*.log

stm32h755_cm7.ld
stm32h755_cm4.ld

das_stm32h755_cm7_hw_test.elf
das_stm32h755_cm7_hw_test.map
das_stm32h755_cm4_link_test.elf
das_stm32h755_cm4_link_test.map

cm7-symbols.txt
cm4-symbols.txt
cm7-elf-size.txt
cm4-elf-size.txt
```

`metadata.txt` records DAS and STM32Cube revisions when available, tool versions, the two linker-script hashes and host information.

## Build-only mode

The existing helper builds the physical CM7 image only:

```bash
./scripts/build_stm32h755.sh \
  /path/to/STM32CubeH7 \
  --clean
```

The full campaign additionally creates the CM4 static linker-smoke build.

## Reusing an existing build

`--no-build` reuses both the existing CM7 hardware image and CM4 link-test image. If either ELF/map is missing, the campaign stops rather than silently skipping that qualification.

`--clean` and `--no-build` are mutually exclusive.

## Recovery

If bad firmware makes normal attachment troublesome, recovery is explicit and separate:

```bash
./scripts/stm32h755_recover.sh
```

The qualification campaign never performs an implicit mass erase.

## Qualification boundary

For CM7, the campaign proves both static placement and physical execution.

For CM4, #3 proves only that a coherent CM4 image can be compiled and linked with the selected memory policy. Physical CPU2 execution requires coordinated dual-core initialization and is therefore tracked by #20.

That distinction is intentional. A valid linker map is evidence about addresses, not evidence that the second processor magically started itself out of politeness.
