# Hardware qualification

DAS treats physical-target testing as part of backend qualification, not as an optional demonstration after the code has already declared itself correct.

Current target:

```text
Board: NUCLEO-H755ZI-Q
MCU:   STM32H755
Core:  Cortex-M7
Debug: ST-LINK + OpenOCD + GDB
```

The test firmware uses DAS APIs for control and exposes a small evidence structure for GDB inspection. GDB also reads hardware state independently where useful.

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

The campaign builds the test image, starts OpenOCD, performs a non-destructive board probe before flashing, programs the firmware, runs automated GDB cases, pauses for required physical wiring, asks for visual LED confirmation, and packages the evidence.

## What is tested

### 1. Board/OpenOCD probe

Before flashing, GDB attaches through OpenOCD and checks the Cortex-M CPUID.

Expected part number:

```text
0xC27 -> Cortex-M7
```

This step is non-destructive. If attachment fails, the campaign stops before programming a new image.

### 2. Flash and firmware bring-up

The campaign:

- loads the ELF;
- runs GDB `compare-sections`;
- resets/runs the target;
- checks that the firmware booted;
- checks that the heartbeat advances;
- checks that no fault/error evidence was recorded;
- checks expected GPIO clocks/modes.

### 3. Cortex-M startup/reset

The campaign validates the reusable Cortex-M reset/runtime path rather than merely inferring it from a successful boot.

GDB deliberately changes test variables placed in `.data` and `.bss`, then resets the target and verifies:

- `.data` is restored from its flash load image;
- `.bss` is cleared to zero;
- SCB VTOR points to `__vector_table_start__`;
- the application reaches `main()` again;
- the heartbeat advances;
- no startup/fault error is recorded.

The test image supplies the STM32H755-specific vector table. The reusable Cortex-M layer owns reset/runtime mechanics and weak core exception defaults only.

### 4. GPIO pull-up

The input test pin is left electrically disconnected and configured with an internal pull-up. The physical input state must read high.

### 5. GPIO pull-down

The same disconnected input is configured with an internal pull-down. The physical input state must read low.

### 6. GPIO loopback low/high

A physical jumper connects a DAS-controlled output to a DAS-controlled input.

The firmware drives low and high and verifies that the input pad follows both states. The signal must leave one MCU pad, travel through the jumper, and return through another input pad.

### 7. GPIO open-drain

Using the same loopback wiring, the output is configured open-drain and the input side observes driven-low and released/high behavior with the configured pull-up path.

### 8. GPIO EXTI rising/falling

The output pin generates physical edges into the input pin. The input is configured for both-edge EXTI operation.

The hardware-test image currently owns Cortex-M NVIC setup directly while DAS owns GPIO-to-EXTI configuration and pending/clear operations. Dedicated Cortex-M NVIC helpers are tracked separately.

The campaign requires evidence of rising and falling interrupt delivery, not merely correctly programmed EXTI registers.

### 9-13. Board LEDs

The campaign checks:

- all LEDs off;
- green only;
- yellow only;
- red only;
- all three blinking together.

Each static state is checked in software/register evidence and then confirmed visually. Blinking is confirmed by an advancing firmware counter plus visual observation.

## Required GPIO wiring

Loopback fixture:

```text
CN10 D4 / PE14 / pin 8   ---- jumper ----   CN10 D3 / PE13 / pin 10
       output                                   input / EXTI13
```

Use one direct jumper only.

Do not connect either test pin to 3V3, 5V or GND unless a future test explicitly instructs otherwise.

For pull-up/down cases, **D3 / PE13 must be electrically disconnected**.

## Board LED mapping

| LED | GPIO | Active state |
| --- | --- | --- |
| LD1 green | PB0 | high |
| LD2 yellow | PE1 | high |
| LD3 red | PB14 | high |

## Expected summary

A complete current campaign contains **13 acceptance points**:

```text
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

Typical contents include:

```text
<timestamp>/
├── summary.txt
├── metadata.txt
├── build.log
├── openocd.log
├── board_probe.log
├── flash_probe.log
├── cortex_m_startup_reset.log
├── GPIO_pull-up.log
├── GPIO_pull-down.log
├── GPIO_loopback_low_high.log
├── GPIO_open-drain.log
├── GPIO_EXTI_rising_falling.log
├── LED_all_off.log
├── LED_green_only.log
├── LED_yellow_only.log
├── LED_red_only.log
├── LED_all_blink.log
├── final_all_off.log
├── das_stm32h755_hw_test.elf
├── das_stm32h755_hw_test.map
├── elf-size.txt
└── symbols.txt
```

The bundle is produced even when the campaign fails, as long as logging has started. `metadata.txt` records the DAS commit, STM32CubeH7 commit when available, tool versions, and host information.

## Build-only mode

```bash
./scripts/build_stm32h755.sh \
  /path/to/STM32CubeH7 \
  --clean
```

Resulting ELF:

```text
build/stm32h755/tests/hardware/stm32h755/das_stm32h755_hw_test.elf
```

## Reusing an existing build

The campaign supports:

```bash
--no-build
```

when an existing hardware-test ELF is already available. `--clean` and `--no-build` are mutually exclusive.

## Recovery

If bad firmware makes normal attachment troublesome, recovery remains a separate explicit operation:

```bash
./scripts/stm32h755_recover.sh
```

The qualification campaign never silently mass-erases the device as a side effect of a failed test.

## Qualification principles

A useful backend test should answer three questions:

1. Did DAS request the right configuration?
2. Did the MCU execute and reflect that configuration?
3. Did the physical signal/resource behave as intended?

Startup adds a fourth question for core/runtime work: did reset reconstruct the expected C runtime state and vector placement?

The campaign deliberately combines GDB evidence, MCU state and physical observation/loopback so a PASS means substantially more than “the ELF linked and one LED did something.”
