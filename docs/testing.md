# Hardware qualification

DAS treats physical target testing as part of backend qualification. The STM32H755 campaign combines host-side linker/image checks with execution on both Cortex-M cores and packages the evidence into one timestamped archive.

Current target:

```text
Board:    NUCLEO-H755ZI-Q
Device:   STM32H755
CPU1:     Cortex-M7
CPU2:     Cortex-M4
Debug:    ST-LINK direct DAP + OpenOCD + GDB
```

Debugger-driven CM4 execution proves the CM4 image and supported device paths on the real CPU2. It does not yet prove production CM7-to-CM4 boot/release, HSEM, shared-memory ownership or cache-coherency policy.

## Testing model

Peripheral development uses two levels of physical testing:

1. a focused qualifier while a peripheral is being implemented or debugged;
2. after that focused test passes, the same firmware/GDB acceptance case is promoted into the main hardware campaign as standing regression coverage.

Current focused scripts remain useful for fast iteration:

```bash
./scripts/stm32h755_uart_test.sh  /path/to/STM32CubeH7
./scripts/stm32h755_timer_test.sh /path/to/STM32CubeH7
```

The full campaign should be rerun whenever shared startup, clock, GPIO, RCC, IRQ, timebase, board-resource or device-backend changes could affect previously qualified functionality.

## Running the full campaign

```bash
./scripts/stm32h755_test_campaign.sh \
  /home/dev/STM32Cube/Repository/STM32CubeH7/ \
  --clean
```

Every run produces a timestamped evidence archive under:

```text
build/stm32h755/campaign/
```

The archive is also produced after a logged failure.

## Fixture choreography

The campaign deliberately groups tests by physical wiring state. Once a jumper is installed, the script does not ask for the same connection again unless a later test genuinely requires a different state.

### Initial setup

Before OpenOCD starts, the campaign asks for one initial hardware setup:

```text
NUCLEO-H755ZI-Q connected through ST-LINK USB

jumper A, install now and leave connected:
Arduino D1 / TX / PB6  <->  Arduino D0 / RX / PB7

D3 / PE13: disconnected
D4 / PE14: disconnected

jumper B: keep ready for the later D4 <-> D3 transition
B1 USER: released
```

D1/D0 remains connected for the entire run. UART therefore needs no later wiring prompt.

The campaign first completes every acceptance point that requires D3 to be electrically free, including both-core pull-up/pull-down qualification.

### Single D4/D3 transition

The campaign then asks exactly once to install jumper B:

```text
CN10 D4 / PE14  <->  CN10 D3 / PE13
```

From that point onward both jumpers remain installed:

```text
D1 / PB6  <-> D0 / PB7     UART fixture
D4 / PE14 <-> D3 / PE13    GPIO + PWM fixture
```

The D4/D3 connection is then reused without further reconnect prompts by:

- CM4 GPIO loopback/open-drain/EXTI;
- CM7 GPIO loopback/open-drain/EXTI;
- CM7 timer/PWM qualification;
- CM4 timer/PWM qualification.

The campaign may reflash/re-arm a core when changing test images. That is software fixture choreography and is not counted as an additional acceptance point.

Never connect the loopback signals to 3V3, 5V or GND.

## Build products

The current campaign builds and archives:

```text
CM7 hardware image
CM7 monotonic-time image
CM7 UART image
CM7 timer/PWM image
CM7 clock-profile image
CM7 board-resource/button image

CM4 hardware image
CM4 monotonic-time image
CM4 UART image
CM4 timer/PWM image

CM7 custom-link smoke image
```

The normal CM7/CM4 images use the selected DAS linker scripts. The custom-link image exists to prove that the linker override propagates through `das::das`.

## Host-only checks

The first three acceptance points do not touch ST-LINK or the physical board:

```text
STM32H755 CM7 memory layout
STM32H755 CM4 memory layout
Custom linker override
```

These are expected to pass even if the NUCLEO is disconnected. Physical qualification begins with the OpenOCD probes.

Default layout expectations:

```text
CM7 vector      0x08000000
CM7 runtime RAM AXI SRAM, stack top 0x24080000

CM4 vector      0x08100000
CM4 runtime RAM D2 SRAM1, stack top 0x30020000

custom CM7      vector at 0x08020000
```

## Dual-core OpenOCD

```text
:3333 -> STM32H755 CPU1 / Cortex-M7
:3334 -> STM32H755 CPU2 / Cortex-M4

CM7 CPUID part -> 0xC27
CM4 CPUID part -> 0xC24
```

## Monotonic-time qualification

Each core verifies:

- missing-source behavior;
- external/application time-source injection;
- wrap-safe elapsed/deadline handling;
- rejection of intervals outside the safe half-range;
- CMSIS SysTick initialization from the live executing-core clock;
- a 100 ms delay measured against DWT cycles within 5%;
- continued execution.

CM7 first selects the qualified 400 MHz board profile. CM4 independently derives its own live core clock.

## Clock qualification

The CM7 clock image verifies the public board-frequency API and the managed STM32H755 clock/power path:

- supported profiles: 64, 200, 300 and 400 MHz;
- every advertised profile applies and reads back correctly;
- 480 MHz is rejected on the current stock-board profile;
- direct-SMPS/VOS readiness is valid;
- final tree is 400 MHz CM7, 200 MHz CM4/AHB and 100 MHz APB1..4.

## Board-resource and B1 qualification

The semantic resource map includes:

```text
B1 USER                  -> PC13
Arduino D3 / D4 fixture  -> PE13 / PE14
ST-LINK VCP              -> PD8 / PD9
Arduino UART             -> PB6 / PB7
Arduino I2C              -> PB8 / PB9
Arduino SPI              -> PA5 / PA6 / PB5, CS PD14
Arduino PWM D4            -> PE14
```

The B1 case uses the public board-button and generic IRQ APIs and checks released, pressed, press EXTI, released again and release EXTI. Mechanical bounce is tolerated; at least one event is required rather than an exact edge count.

## UART qualification

The persistent fixture is:

```text
Arduino D1 / TX / PB6  <->  Arduino D0 / RX / PB7
```

Both cores verify:

- finite receive timeout;
- semantic board-resource and opaque-handle setup;
- baud generation from the live clock tree;
- 115200 8N1;
- 57600 8E2;
- 38400 7O1;
- 34 deterministic bytes with exact application-byte equality.

The 7O1 case protects the contract that `data_bits` excludes parity. STM32 parity storage must not leak into the byte returned by DAS.

## GPIO qualification

Both cores verify:

```text
pull-up                 D3 electrically free
pull-down               D3 electrically free
loopback low/high       D4 -> D3 connected
open-drain              D4 -> D3 connected
EXTI rising/falling     D4 -> D3 connected
```

The EXTI case also validates the public `das_irq_*()` controller path: enable state, priority round-trip, software pending set/query/clear and real physical edge delivery.

## Timer/PWM qualification

Issue #10 keeps a focused qualifier but is also part of the main campaign after its first successful physical run.

Each core runs a dedicated timer/PWM image using the already-installed D4/D3 fixture. The image verifies:

- a 1 kHz periodic timer derived from the live DAS clock model;
- start/stop and counter behavior;
- TIM2 update interrupt delivery through generic `das_irq_t` control;
- 100 update intervals measured with DWT cycles within 5%;
- a 1 kHz PWM output on semantic Arduino D4;
- physical D4-to-D3 observation at 25%, 50% and 75% duty;
- PWM frequency/duty readback and continued execution.

D3 is deliberately used as a GPIO observation input. Input-capture support is not introduced merely to make the test fixture more elaborate.

## Board LED checks

The visual CM7 checks remain:

```text
all LEDs off
green only
yellow only
red only
all three blinking
```

CM4 already proves physical GPIO output through the electrical loopback path, so duplicating the five human LED checks on CPU2 adds ceremony rather than coverage.

## Expected 32-case summary

The timer-integrated campaign contains **32 acceptance points**:

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
CM7 UART loopback                 PASS
CM4 UART loopback                 PASS

CM7 CMSIS/GPIO bring-up           PASS
CM7 Cortex-M startup/reset        PASS
CM7 GPIO pull-up                  PASS
CM7 GPIO pull-down                PASS

CM4 CMSIS/GPIO bring-up           PASS
CM4 Cortex-M startup/reset        PASS
CM4 GPIO pull-up                  PASS
CM4 GPIO pull-down                PASS

CM4 GPIO loopback low/high        PASS
CM4 GPIO open-drain               PASS
CM4 GPIO EXTI rising/falling      PASS

CM7 GPIO loopback low/high        PASS
CM7 GPIO open-drain               PASS
CM7 GPIO EXTI rising/falling      PASS
CM7 LED all off                   PASS
CM7 LED green only                PASS
CM7 LED yellow only               PASS
CM7 LED red only                  PASS
CM7 LED all blink                 PASS

CM7 timer/PWM                     PASS
CM4 timer/PWM                     PASS
```

The most recent fully qualified baseline before timer integration remains the 30/30 UART campaign. The 32-case state becomes the new baseline only after an archive from the exact integration head passes completely.

## Evidence bundle

The archive includes the summary, metadata, build logs, OpenOCD log, per-case GDB logs, ELF/map files, symbol/size dumps, linker scripts and the dedicated UART/timer/PWM images. Timer evidence is stored as:

```text
CM7_timer_PWM.log
CM4_timer_PWM.log
das_stm32h755_cm7_timer_test.elf/.map
das_stm32h755_cm4_timer_test.elf/.map
```

`--no-build` requires every expected image, including UART and timer/PWM images. Missing artifacts are errors rather than silently reducing coverage.

## Recovery

Explicit destructive recovery remains separate:

```bash
./scripts/stm32h755_recover.sh
```

The normal campaign never performs an implicit mass erase.

## Qualification boundary

After a successful 32-case run DAS can claim physically regression-qualified linker, startup, clock/power, monotonic time, semantic board resources, GPIO/IRQ, polling UART and the #10 periodic-timer/PWM baseline on both cores.

That still does not imply DMA, input capture, production dual-core lifecycle/HSEM/shared-memory coordination, or unimplemented SPI/I2C transfers. Those remain separate work.