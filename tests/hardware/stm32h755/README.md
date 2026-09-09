# STM32H755 physical bring-up

Target: NUCLEO-H755ZI-Q, CM7.

This first DAS campaign intentionally uses only CMSIS core/device headers. It does not link STM32 HAL or LL. OpenOCD attaches under reset at a conservative 400 kHz SWD clock, flashes one freestanding test image, and lets GDB verify software/register evidence. LED behavior is additionally accepted by the person looking at the board.

The current board mapping is:

- green LD1: PB0, active high
- yellow LD2: PE1, active high
- red LD3: PB14, active high

Run everything from the repository root:

```bash
./scripts/stm32h755_test_campaign.sh /path/to/STM32CubeH7 --clean
```

The campaign first performs a non-destructive Cortex-M7/OpenOCD probe, then flashes the test image, verifies that GPIO clocks and output modes were configured, and then asks for visual confirmation of all-off, green-only, yellow-only, red-only, and all-blinking states. Raw OpenOCD/GDB logs are stored below `build/stm32h755/campaign/`.

Build without running hardware tests:

```bash
./scripts/build_stm32h755.sh /path/to/STM32CubeH7 --clean
```

If a bad image makes normal attachment troublesome, explicitly erase both flash banks with:

```bash
./scripts/stm32h755_recover.sh
```

The recovery command is intentionally separate because mass erase is destructive and should never be a casual side effect of a test runner.
