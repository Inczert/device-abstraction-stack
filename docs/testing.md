# Hardware qualification

DAS treats physical-target testing as part of backend qualification, not as an optional demonstration after the code has already declared itself correct.

Current target:

```text
Board: NUCLEO-H755ZI-Q
MCU:   STM32H755
Core:  Cortex-M7
Debug: ST-LINK + OpenOCD + GDB
```

## Running the campaign

```bash
./scripts/stm32h755_test_campaign.sh \
  /path/to/STM32CubeH7 \
  --clean
```

The campaign builds the test image, validates the generated memory layout, starts OpenOCD, probes the board, flashes the ELF, runs automated GDB cases, pauses for GPIO wiring, asks for LED confirmation, and packages the evidence.

## What is tested

### 1. STM32H755 memory layout

Before touching the board, the campaign runs:

```text
scripts/check_stm32h755_memory_layout.sh
```

against the generated ELF and map file. It checks:

- required linker/startup symbols exist in the ELF and map;
- vector table begins at `0x08000000`;
- `.data` load image resides in flash;
- `.data` and `.bss` reside in AXI SRAM;
- stack top is `0x24080000`;
- heap/static bounds do not overlap the reserved stack.

The hardware-test target links the same reusable script exposed to applications through `das::linker`:

```text
cmake/targets/stm32h755_cm7.ld
```

### 2. Board/OpenOCD probe

GDB attaches through OpenOCD and verifies Cortex-M7 CPUID before flashing. Failure stops the campaign without programming a new image.

### 3. Flash and firmware bring-up

The ELF is loaded and checked with `compare-sections`; the campaign then verifies boot, heartbeat, fault state, and expected GPIO clocks/modes.

### 4. Cortex-M startup/reset

GDB deliberately corrupts variables in `.data` and `.bss`, resets the target, and verifies:

- `.data` is restored from flash;
- `.bss` is cleared;
- SCB VTOR equals `__vector_table_start__`;
- `main()` is reached again;
- heartbeat advances;
- no startup/fault error is recorded.

This links the build-time memory contract to actual reset/runtime behavior on silicon.

### 5. GPIO pull-up

With D3/PE13 electrically disconnected, the internal pull-up must produce a high physical input state.

### 6. GPIO pull-down

The same disconnected input configured pull-down must read low.

### 7. GPIO loopback low/high

A physical jumper carries low and high levels from D4/PE14 output to D3/PE13 input.

### 8. GPIO open-drain

The same loopback verifies driven-low and released/high open-drain behavior.

### 9. GPIO EXTI rising/falling

The output generates physical edges into the EXTI input and the campaign requires rising and falling IRQ evidence.

### 10-14. Board LEDs

The campaign checks all-off, green-only, yellow-only, red-only and synchronized blinking. Software/register evidence is combined with human visual confirmation.

## Required GPIO wiring

```text
CN10 D4 / PE14 / pin 8   ---- jumper ----   CN10 D3 / PE13 / pin 10
       output                                   input / EXTI13
```

For pull-up/down tests D3/PE13 must be disconnected. Do not connect either test pin to 3V3, 5V or GND unless a future test explicitly requires it.

## Expected summary

A complete campaign now contains **14 acceptance points**:

```text
STM32H755 memory layout          PASS
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

Any failure makes the campaign exit nonzero.

## Evidence bundle

Every campaign creates a timestamped directory and `.tar.gz` archive under:

```text
build/stm32h755/campaign/
```

Important contents now include:

```text
summary.txt
metadata.txt
build.log
memory_layout.log
stm32h755_cm7.ld
das_stm32h755_hw_test.map
das_stm32h755_hw_test.elf
symbols.txt
elf-size.txt
openocd.log
board_probe.log
flash_probe.log
cortex_m_startup_reset.log
GPIO_*.log
LED_*.log
final_all_off.log
```

The archive is produced even on failure after logging starts. `metadata.txt` records the DAS commit, tool versions, STM32CubeH7 commit when available, and the linker-script path/hash.

## Build-only mode

```bash
./scripts/build_stm32h755.sh /path/to/STM32CubeH7 --clean
```

## Reusing an existing build

Use `--no-build` when the expected ELF/map already exists. It cannot be combined with `--clean`.

## Recovery

Destructive recovery remains a separate explicit operation:

```bash
./scripts/stm32h755_recover.sh
```

A failed qualification run never silently mass-erases the target.

## Qualification principles

A useful DAS acceptance test should answer:

1. Did build/link policy put code and data where intended?
2. Did reset reconstruct the expected runtime state?
3. Did DAS request and execute the intended peripheral configuration?
4. Did the physical resource behave correctly?

Combining linker-map evidence, GDB state, physical loopback and visual observation gives failures enough context to diagnose instead of merely producing a red badge and emotional damage.
