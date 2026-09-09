# STM32H755 physical bring-up

Target: NUCLEO-H755ZI-Q, CM7.

This DAS campaign intentionally uses only CMSIS core/device headers. It does not link STM32 HAL or LL. OpenOCD attaches under reset at a conservative 400 kHz SWD clock, flashes one freestanding test image, and lets GDB verify software/register evidence. LED behavior is additionally accepted by the person looking at the board.

The board LED mapping is:

- green LD1: PB0, active high
- yellow LD2: PE1, active high
- red LD3: PB14, active high

The GPIO electrical-path fixture uses two Arduino-header pins:

- D4: PE14, test output
- D3: PE13, test input / EXTI13

The runner asks for wiring changes at the appropriate time. For pull-up/down qualification, D3 must be electrically disconnected. For loopback, open-drain, and EXTI qualification, connect exactly one jumper from D4 to D3. The open-drain test uses D3's internal pull-up, so no external resistor is required for this campaign.

Run everything from the repository root:

```bash
./scripts/stm32h755_test_campaign.sh /path/to/STM32CubeH7 --clean
```

The campaign performs:

1. a non-destructive Cortex-M7/OpenOCD probe;
2. flash/startup/CMSIS GPIO bring-up validation;
3. floating-input pull-up and pull-down checks;
4. physical D4-to-D3 push-pull loopback at low and high levels;
5. open-drain drive-low/release-high validation;
6. rising and falling EXTI13 interrupt validation through the physical jumper;
7. visual plus register-backed LED output/toggle checks.

Raw OpenOCD/GDB logs are stored below `build/stm32h755/campaign/`.

Build without running hardware tests:

```bash
./scripts/build_stm32h755.sh /path/to/STM32CubeH7 --clean
```

If a bad image makes normal attachment troublesome, explicitly erase both flash banks with:

```bash
./scripts/stm32h755_recover.sh
```

The recovery command is intentionally separate because mass erase is destructive and should never be a casual side effect of a test runner.
