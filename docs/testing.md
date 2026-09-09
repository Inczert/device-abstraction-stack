# Hardware qualification

DAS treats physical-target testing as part of backend qualification, not as an optional demonstration after the code already declared itself correct.

The current campaign targets:

```text
Board: NUCLEO-H755ZI-Q
MCU:   STM32H755
Core:  Cortex-M7
Debug: ST-LINK + OpenOCD + GDB
```

The test firmware uses the public DAS API for control and exposes a small evidence structure for GDB inspection. GDB also reads hardware state independently where useful.

## Running the campaign

From the repository root:

```bash
./scripts/stm32h755_test_campaign.sh \
  /path/to/STM32CubeH7 \
  --clean
```

Example with the default STM32Cube installation layout:

```bash
./scripts/stm32h755_test_campaign.sh \
  /home/dev/STM32Cube/Repository/STM32CubeH7/ \
  --clean
```

The campaign builds the test image, starts OpenOCD, performs a non-destructive board probe before flashing, programs the firmware, runs automated GDB cases, pauses for required physical wiring, and asks for visual LED confirmation.

## What is tested

### Board/OpenOCD probe

Before flashing, GDB attaches through OpenOCD and checks the Cortex-M CPUID.

For the target core the expected Cortex-M part number is:

```text
0xC27  -> Cortex-M7
```

This step is intentionally non-destructive. If attachment fails, the campaign stops before programming a new image.

### Flash and firmware bring-up

The campaign:

- loads the ELF;
- runs GDB `compare-sections`;
- resets/runs the target;
- checks that the firmware booted;
- checks that the heartbeat advances;
- checks that no HardFault/error evidence was recorded;
- checks the expected GPIO clocks/modes.

### GPIO pull-up

The input test pin is left electrically disconnected and configured with an internal pull-up. The physical input state must read high.

### GPIO pull-down

The same disconnected input is configured with an internal pull-down. The physical input state must read low.

### GPIO loopback low/high

A physical jumper connects a DAS-controlled output to a DAS-controlled input.

The firmware drives low and high and verifies that the input pad follows both states.

This tests more than `ODR`: the signal must leave one MCU pad, travel through the jumper, and return through another input pad.

### GPIO open-drain

Using the same loopback wiring, the output is configured open-drain and the input side observes driven-low and released/high behavior with the configured pull-up path.

### GPIO EXTI rising/falling

The output pin generates physical edges into the input pin. The input is configured for both-edge EXTI operation.

The hardware-test application owns the Cortex-M NVIC/vector setup while DAS owns GPIO-to-EXTI configuration and pending/clear operations.

The campaign requires evidence of rising and falling interrupt delivery, not merely correctly programmed EXTI registers.

### Board LEDs

The campaign checks:

- all LEDs off;
- green only;
- yellow only;
- red only;
- all three blinking together.

Each static state is checked in software/register evidence and then confirmed visually by the person looking at the board.

Blinking is confirmed by an advancing firmware blink counter plus visual observation.

## Required GPIO wiring

The loopback fixture uses two pins on connector CN10:

```text
CN10 D4 / PE14 / pin 8   ---- jumper ----   CN10 D3 / PE13 / pin 10
       output                                   input / EXTI13
```

Use one direct jumper only.

Do not connect either test pin to:

```text
3V3
5V
GND
```

unless a future test explicitly instructs otherwise.

For the pull-up and pull-down cases, **D3 / PE13 must be electrically disconnected**.

The campaign pauses before each wiring state and prints the exact required connection.

## Board LED mapping

| LED | GPIO | Active state |
| --- | --- | --- |
| LD1 green | PB0 | high |
| LD2 yellow | PE1 | high |
| LD3 red | PB14 | high |

## Expected summary

A complete current campaign contains 12 acceptance points:

```text
Board/OpenOCD probe             PASS
CMSIS/GPIO bring-up             PASS
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

Typical contents:

```text
<timestamp>/
├── summary.txt
├── metadata.txt
├── build.log
├── openocd.log
├── board_probe.log
├── flash_probe.log
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

The bundle is produced even when the campaign fails, as long as the campaign reached the logging setup. That makes it suitable for attaching to an issue or passing to another developer for diagnosis.

`metadata.txt` records useful reproduction context such as:

- DAS commit;
- STM32CubeH7 commit when available;
- compiler version;
- CMake version;
- OpenOCD version;
- GDB version;
- host kernel/platform information.

## Build-only mode

Build the hardware-test image without executing it:

```bash
./scripts/build_stm32h755.sh \
  /path/to/STM32CubeH7 \
  --clean
```

The resulting ELF is:

```text
build/stm32h755/tests/hardware/stm32h755/das_stm32h755_hw_test.elf
```

## Reusing an existing build

The campaign supports:

```bash
--no-build
```

when an existing hardware-test ELF is already available in the selected build directory.

`--clean` and `--no-build` are intentionally mutually exclusive.

## Recovery

If a bad firmware image makes normal attachment troublesome, recovery is a separate explicit operation:

```bash
./scripts/stm32h755_recover.sh
```

The recovery script performs a destructive flash mass erase using the same conservative OpenOCD connection strategy.

The qualification campaign never silently mass-erases the device as a side effect of a failed test.

## Test design principles

A useful backend test should answer three different questions:

1. **Did DAS request the right configuration?**
2. **Did the MCU actually execute and reflect that configuration?**
3. **Did the physical signal/resource behave as intended?**

Register-only tests answer the first two imperfectly. Visual-only tests answer the third with poor diagnostics. The campaign deliberately combines GDB evidence, MCU state, and physical observation/loopback so a PASS means substantially more than “the ELF linked and one LED did something.”
