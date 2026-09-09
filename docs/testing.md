# Hardware qualification

DAS treats target testing as part of backend qualification. The STM32H755 campaign combines host-side linker/image checks with physical execution on both Cortex-M cores and packages all evidence into one archive.

Current target:

```text
Board:    NUCLEO-H755ZI-Q
Device:   STM32H755
CPU1:     Cortex-M7
CPU2:     Cortex-M4
Debug:    ST-LINK direct DAP + OpenOCD + GDB
```

Debugger-driven CM4 execution proves that the CM4 image and supported core/device paths work on the real CPU2. It does not yet prove production CM7-to-CM4 boot/release, HSEM or shared-memory coordination.

## Running the campaign

```bash
./scripts/stm32h755_test_campaign.sh \
  /home/dev/STM32Cube/Repository/STM32CubeH7/ \
  --clean
```

Every run produces one timestamped archive under:

```text
build/stm32h755/campaign/
```

The archive is also created when a test fails after campaign logging has started.

## Build products

The expanded campaign builds:

```text
CM7 hardware image
CM7 monotonic-time image
CM7 clock-profile image
CM7 board-resource/button image
CM4 hardware image
CM4 monotonic-time image
CM7 custom-link smoke image
```

The normal CM7 and CM4 hardware images use their respective default linker scripts. The custom-link image uses `tests/link/stm32h755/custom_cm7.ld` and exists solely to prove the linker-script override propagated through `das::das`.

## Host-only acceptance points

The first three checks do not access ST-LINK or the physical board:

```text
STM32H755 CM7 memory layout
STM32H755 CM4 memory layout
Custom linker override
```

They validate ELF/map placement, runtime symbols and heap/stack boundaries. These three checks are expected to pass even when the NUCLEO is disconnected. Hardware qualification starts with the OpenOCD probes.

Default layout expectations:

```text
CM7 vector      0x08000000
CM7 runtime RAM AXI SRAM, stack top 0x24080000

CM4 vector      0x08100000
CM4 runtime RAM D2 SRAM1, stack top 0x30020000

custom CM7      vector at 0x08020000
```

## Dual-core OpenOCD

The physical phase uses `scripts/openocd_h755_dual_core.cfg` with ST-LINK direct DAP.

```text
:3333 -> STM32H755 CPU1 / Cortex-M7
:3334 -> STM32H755 CPU2 / Cortex-M4
```

Expected CPUID parts:

```text
CM7 -> 0xC27
CM4 -> 0xC24
```

## Monotonic-time qualification

A dedicated time image is run on each core.

Each test validates:

- no-source delay returns `DAS_ERROR_NOT_READY`;
- external/application time-source injection;
- elapsed time across `UINT32_MAX` wrap;
- wrap-safe deadline creation/comparison;
- rejection of intervals beyond the half-range limit;
- default CMSIS SysTick initialization from the live executing-core clock;
- a 100 ms delay measured against DWT cycles within 5%;
- continued execution after the timing case.

CM7 first selects the qualified 400 MHz board profile. CM4 independently uses its live core clock.

## Clock-profile qualification

The dedicated CM7 clock image exercises the public `das_clock_*()` path. On the stock NUCLEO-H755ZI-Q it must:

- advertise 64, 200, 300 and 400 MHz;
- successfully apply/read back every advertised profile;
- reject 480 MHz;
- configure the direct-SMPS board power path;
- confirm power/VOS readiness;
- finish at 400 MHz CM7, 200 MHz CM4/AHB and 100 MHz APB1..4;
- continue executing after the transitions.

## Board-resource and user-button qualification

Issue #15 adds a dedicated CM7 board-resource/button image.

Before the physical button interaction, firmware validates the semantic resource map used by DAS:

```text
B1 USER                  -> PC13
Arduino D3 / D4 fixture  -> PE13 / PE14
ST-LINK VCP              -> PD8 / PD9
Arduino UART             -> PB6 / PB7
Arduino I2C              -> PB8 / PB9
Arduino SPI              -> PA5 / PA6 / PB5, CS PD14
```

The campaign then qualifies B1 through the **public board-button API** and generic DAS IRQ controller API.

Sequence:

1. leave the blue B1 USER button released while the button image is loaded;
2. the initial state must read logically released;
3. when prompted, press and hold B1 and press ENTER while holding it;
4. firmware must report logically pressed and at least one press EXTI event;
5. release B1 and press ENTER when prompted;
6. firmware must report logically released and at least one release EXTI event.

Mechanical bounce is intentionally tolerated. The qualification requires one or more press/release events, not an exact count.

The current button case runs on CM7 because #15 is board-resource qualification. The lower-level GPIO/EXTI implementation and generic IRQ path are already independently physically qualified on both CM7 and CM4.

## Core startup/GPIO qualification

Both CM7 and CM4 independently run the existing hardware image.

Bring-up checks include:

- ELF programming and `compare-sections`;
- firmware boot/heartbeat;
- reusable Cortex-M startup;
- `.data` restoration and `.bss` clearing after reset;
- VTOR equal to the linked vector-table address;
- no startup/fault evidence.

The same five automated GPIO electrical tests then run on each core:

```text
pull-up
pull-down
D4 -> D3 loopback low/high
open-drain
EXTI rising/falling
```

Fixture:

```text
CN10 D4 / PE14 / pin 8 ---- jumper ---- CN10 D3 / PE13 / pin 10
```

Pull tests require D3 to be disconnected. Loopback/open-drain/EXTI require the D4-to-D3 jumper. Do not connect either pin to 3V3, 5V or GND.

The EXTI case also qualifies the public `das_irq_*()` path: source-to-controller resolution, enable state, priority set/get, software pending set/query/clear, and real physical edge delivery.

## Board LED checks

The visual LED qualification remains on CM7:

```text
all LEDs off
green only
yellow only
red only
all three blinking
```

Board mapping:

| LED | GPIO | Active state |
| --- | --- | --- |
| LD1 green | PB0 | high |
| LD2 yellow | PE1 | high |
| LD3 red | PB14 | high |

CM4 already drives GPIO physically through the loopback tests, so duplicating five human visual LED confirmations on CPU2 would add ceremony rather than meaningful coverage.

## Expected 28-case summary

A complete #15 campaign contains **28 acceptance points**:

```text
STM32H755 CM7 memory layout       PASS   [host/static]
STM32H755 CM4 memory layout       PASS   [host/static]
Custom linker override            PASS   [host/static]

CM7 OpenOCD probe                 PASS
CM4 OpenOCD probe                 PASS
CM7 monotonic timebase            PASS
CM4 monotonic timebase            PASS
CM7 HSI/PLL 400MHz clock          PASS
CM7 user button input/EXTI        PASS

CM7 CMSIS/GPIO bring-up           PASS
CM7 Cortex-M startup/reset        PASS
CM7 GPIO pull-up                  PASS
CM7 GPIO pull-down                PASS
CM7 GPIO loopback low/high        PASS
CM7 GPIO open-drain               PASS
CM7 GPIO EXTI rising/falling      PASS
CM7 LED all off                   PASS
CM7 LED green only                PASS
CM7 LED yellow only               PASS
CM7 LED red only                  PASS
CM7 LED all blink                 PASS

CM4 CMSIS/GPIO bring-up           PASS
CM4 Cortex-M startup/reset        PASS
CM4 GPIO pull-up                  PASS
CM4 GPIO pull-down                PASS
CM4 GPIO loopback low/high        PASS
CM4 GPIO open-drain               PASS
CM4 GPIO EXTI rising/falling      PASS
```

The script exits nonzero when an acceptance point fails.

## Evidence bundle

Typical archive contents include:

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
CM7_monotonic_timebase.log
CM4_monotonic_timebase.log
CM7_clock_HSI_PLL_400.log
CM7_button_setup.log
CM7_button_pressed.log
CM7_button_released.log
CM7_GPIO_*.log
CM4_GPIO_*.log
CM7_LED_*.log

das_stm32h755_cm7_hw_test.elf/.map
das_stm32h755_cm7_time_test.elf/.map
das_stm32h755_cm7_clock_test.elf/.map
das_stm32h755_cm7_button_test.elf/.map
das_stm32h755_cm4_hw_test.elf/.map
das_stm32h755_cm4_time_test.elf/.map
das_stm32h755_custom_link_test.elf/.map

symbol and size dumps for each generated image
linker scripts used by the run
```

## Reusing an existing build

`--no-build` reuses all expected images, including the button image. Missing ELF/map files are treated as errors rather than silently reducing coverage.

`--clean` and `--no-build` are mutually exclusive.

## Recovery

Explicit destructive recovery remains separate:

```bash
./scripts/stm32h755_recover.sh
```

The normal campaign never performs an implicit mass erase.

## Qualification boundary

After a successful 28-case campaign DAS can claim that the semantic NUCLEO board-resource mapping and B1 input/EXTI path are physically qualified in addition to the already-qualified linker, clock, time, GPIO and IRQ foundations.

It still cannot claim production CM7-to-CM4 lifecycle/HSEM/shared-memory coordination or unimplemented UART/I2C/SPI data transfers. Those remain separate features.
