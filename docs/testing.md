# Hardware qualification

DAS treats physical target testing as part of backend qualification. The STM32H755 campaign combines static linker/image checks with real execution on both Cortex-M cores and packages the evidence into one timestamped archive.

Current target:

```text
Board:    NUCLEO-H755ZI-Q
Device:   STM32H755
CPU1:     Cortex-M7
CPU2:     Cortex-M4
Debug:    ST-LINK direct DAP + OpenOCD + GDB
```

Debugger-driven CM4 execution proves the CM4 image and supported device paths on the real CPU2. It does not yet prove production CM7-to-CM4 boot/release, HSEM or shared-memory ownership policy.

## Qualified baseline

The latest completed standing regression baseline is **39/39 PASS**:

```text
DAS commit:       f6b65672d9ae69cf28cd574d0dbba01cf875d8dc
UTC start:        2026-09-13T15:28:45Z
STM32CubeH7:      f5c0b7a2b1f6eb26fde150f72edb2d7deb647066
PASS:             39
FAIL:             0
Exit code:        0
Evidence archive: das-stm32h755-campaign-20260913T152845Z.tar.gz
```

The campaign includes the CM7 Ethernet Layer-2 case in addition to the previously qualified dual-core linker/startup/clock/time/GPIO/UART/SPI/I2C/DMA/timer paths.

That recorded run predates the explicit Ethernet cable unplug/replug phase now implemented on `develop`. The connected raw-TX/RX baseline remains qualified; the extended Ethernet case requires one physical rerun before link-loss/recovery itself is added to the qualified claim.

## Testing model

Peripheral development uses two levels of physical testing:

1. a focused qualifier while a peripheral is being implemented or debugged;
2. after that focused test passes, the same acceptance path is promoted into the main campaign as standing regression coverage.

Focused scripts remain useful for iteration:

```bash
./scripts/stm32h755_uart_test.sh  /path/to/STM32CubeH7
./scripts/stm32h755_timer_test.sh /path/to/STM32CubeH7
./scripts/stm32h755_spi_test.sh   /path/to/STM32CubeH7
./scripts/stm32h755_i2c_test.sh   /path/to/STM32CubeH7
./scripts/stm32h755_dma_test.sh   /path/to/STM32CubeH7
./scripts/stm32h755_eth_test.sh   /path/to/STM32CubeH7
```

The Ethernet scripts default to Linux host interface `enp0s31f6`. Override it with `--iface` for the focused qualifier, `--eth-iface` for the full campaign, or `DAS_ETH_IFACE` for either.

The full campaign should be rerun whenever shared startup, clock, GPIO, RCC, IRQ, timebase, DMA/cache, board-resource or device-backend changes could affect previously qualified behavior.

## Running the full campaign

```bash
./scripts/stm32h755_test_campaign.sh \
  /home/dev/STM32Cube/Repository/STM32CubeH7/ \
  --clean
```

Use `--eth-iface <linux-interface>` only when the host Ethernet interface differs from the default `enp0s31f6`.

Every run produces a timestamped evidence directory and archive under:

```text
build/stm32h755/campaign/
```

The archive is also produced after a logged failure.

## Initial hardware setup

Install the persistent fixtures before starting the campaign:

```text
NUCLEO-H755ZI-Q connected through ST-LINK USB

jumper A, leave connected:
Arduino D1 / TX / PB6  <->  Arduino D0 / RX / PB7
UART loopback

jumper B, leave connected:
Arduino D11 / MOSI / PB5  <->  Arduino D12 / MISO / PA6
SPI polling + SPI-DMA loopback

jumper C, leave connected:
Arduino D15 / PB8 / I2C_A_SCL  <->  Zio D69 / PF14 / I2C_B_SCL
                                      CN9 pin 19

jumper D, leave connected:
Arduino D14 / PB9 / I2C_A_SDA  <->  Zio D68 / PF15 / I2C_B_SDA
                                      CN9 pin 21

D3 / PE13: disconnected initially
D4 / PE14: disconnected initially
jumper E: keep ready for the later D4 <-> D3 transition
B1 USER: released

Ethernet:
JP6 fitted
JP7 fitted
board RJ45 CN14  <->  host PC Ethernet port
host interface defaults to enp0s31f6
keep the cable connected initially; the Ethernet qualifier later prompts for one
unplug/replug cycle and automatically verifies both transitions
```

The Ethernet case uses raw Layer-2 frames. It does not require IPv4/IPv6 configuration, DHCP or a network stack.

Leave SPI D13/SCK/PA5 and D10/CS/PD14 otherwise unconnected. Never connect the loopback signal pins to 3V3, 5V or GND.

After the D3-free pull tests, the campaign asks once for the final fixture transition:

```text
CN10 D4 / PE14  <->  CN10 D3 / PE13
```

That jumper then remains connected for GPIO loopback/open-drain/EXTI and timer/PWM qualification on both cores.

## Host/static acceptance points

The first three cases do not require the board:

```text
STM32H755 CM7 memory layout
STM32H755 CM4 memory layout
Custom linker override
```

Default layout expectations are:

```text
CM7 vector      0x08000000
CM7 runtime RAM AXI SRAM, stack top 0x24080000

CM4 vector      0x08100000
CM4 runtime RAM D2 SRAM1, stack top 0x30020000

custom CM7      vector at 0x08020000
```

## Physical qualification scope

### Core/startup and time

OpenOCD exposes:

```text
:3333 -> CM7 / CPU1, CPUID part 0xC27
:3334 -> CM4 / CPU2, CPUID part 0xC24
```

Both cores qualify their image/startup path and monotonic-time behavior. Time qualification covers missing-source handling, application-source injection, wrap-safe interval/deadline logic, SysTick initialization from the live core clock and a DWT-measured 100 ms delay within 5%.

### Clock/power

CM7 qualifies the stock-board 64/200/300/400 MHz profiles, rejection of 480 MHz, direct-SMPS/VOS readiness and final live-clock readback at 400 MHz CM7, 200 MHz HCLK/CM4 and 100 MHz APB1..4.

### Board button and GPIO/IRQ

B1 USER qualifies polling plus press/release EXTI through the semantic board API. CM7 and CM4 qualify GPIO pull-up/down, physical D4-to-D3 low/high loopback, open-drain behavior and rising/falling EXTI. The EXTI path also checks generic `das_irq_t` enable, priority and pending behavior.

The CM7 visual LED checks remain:

```text
all LEDs off
green only
yellow only
red only
all three blinking
```

### UART

Fixture:

```text
Arduino D1 / TX / PB6  <->  Arduino D0 / RX / PB7
```

Both cores qualify finite receive timeout, semantic route setup, live baud generation, 115200 8N1, 57600 8E2, 38400 7O1 and exact equality across 34 deterministic bytes.

### SPI

Fixture:

```text
Arduino D11 / MOSI / PB5  <->  Arduino D12 / MISO / PA6
```

Both cores qualify modes 0..3, MSB/LSB first, 1/2/4/8 MHz requested SCK, lengths 1/7/31/64, receive-only fill, transmit-only discard, explicit active-low chip select and 111 deterministic looped-back bytes.

### DMA/cache

The DMA case reuses the SPI fixture. Both cores qualify generic DMA allocation/configuration/release, IRQ resolution, 256-byte memory-to-memory integrity, completion/error state and a 192-byte full-duplex SPI1 DMA transfer at 4 MHz.

CM7 additionally qualifies D-cache availability/enabled state, 32-byte line size and explicit clean/invalidate ownership. CM4 qualifies the same public maintenance API with expected no-D-cache behavior.

The generic `das_dma_t` backend uses DMA1/DMAMUX1. Ethernet uses the Ethernet peripheral's own descriptor-based DMA engine; it is not routed through `das_dma_t`.

### I2C

The test uses I2C1 as the DAS controller and I2C4 as a test-only target:

```text
D15 / PB8 / I2C1_SCL  <->  D69 / PF14 / I2C4_SCL
D14 / PB9 / I2C1_SDA  <->  D68 / PF15 / I2C4_SDA
                                  target 0x52
```

Each core qualifies 100/400 kHz timing, expected probe/NACK behavior, physical write/read, repeated-START write/read, 70 deterministic bytes and zero target-side bus/arbitration/overrun errors.

### Timer/PWM

With D4 connected to D3, both cores qualify a 1 kHz periodic timer, start/stop/counter behavior, TIM2 update IRQ delivery, 100 measured intervals within 5%, and a 1 kHz PWM output observed physically at 25/50/75% duty.

### Ethernet Layer 2

The current Ethernet ownership model is CM7-only. The full campaign ends the long-lived dual-core OpenOCD session and invokes the self-contained Ethernet qualifier.

Fixture:

```text
NUCLEO-H755ZI-Q CN14 RJ45  <->  host PC Ethernet port
JP6 fitted
JP7 fitted
```

The current Ethernet qualifier performs this sequence on one firmware boot and one DAS Ethernet initialization:

```text
initial carrier/link up
        -> operator unplugs cable
host carrier down + STM32 link down
        -> operator reconnects cable
host carrier up + STM32 negotiated link recovery
        -> raw TX/RX integrity traffic after recovery
```

The script waits up to 60 seconds for each physical transition by default. Linux carrier state proves the wire transition, while batch GDB verifies the running STM32 reports down and later negotiated up state. The firmware is resumed after each observation; Ethernet is not reinitialized between unplug and recovery.

The accepted 2026-09-13 campaign run, before this extension, recorded:

```text
host interface:                  enp0s31f6
host carrier:                    PASS
PHY link:                        100 Mbps / full duplex
STM32 -> host validation:        5/5 frames
TX interval:                     0.995806 .. 0.995980 s
host -> STM32 injection:         64 frames
firmware RX integrity count:     64
firmware RX integrity errors:    0
last test sequence:              64
DAS final result:                DAS_OK
```

STM32-to-host qualification uses broadcast EtherType `0x88B5` frames from MAC `02:00:00:00:00:01` with payload `DAS ETH L2 test`. Host-to-STM32 qualification uses EtherType `0x88B6`, monotonic sequence numbers and deterministic payload data validated by firmware.

The GDB evidence for the accepted connected run reported:

```text
ETH_LINK up=1 speed_mbps=100 duplex=2
ETH_TX count=8
ETH_RX count=66 bytes=4229 test_count=64 test_errors=0 last_sequence=64
ETH_RESULT last_result=0
RESULT: PASS
```

This qualifies RMII routing, LAN8742A/MDIO link management, MAC configuration, TX/RX descriptor recycling, raw frame transfer and CM7 D-cache coherency for the polling Layer-2 connected baseline. The newly added link-loss/recovery phase must pass once on hardware before adding that behavior to the qualified claim.

The Ethernet case still counts as one acceptance point, so the standing campaign remains a 39-case campaign rather than inflating the case count every time one peripheral gains a stronger internal check.

## 39-case acceptance summary

```text
STM32H755 CM7 memory layout        PASS
STM32H755 CM4 memory layout        PASS
Custom linker override             PASS
CM7 OpenOCD probe                  PASS
CM4 OpenOCD probe                  PASS
CM7 monotonic timebase             PASS
CM4 monotonic timebase             PASS
CM7 HSI/PLL 400MHz clock           PASS
CM7 user button input/EXTI         PASS
CM7 UART loopback                  PASS
CM4 UART loopback                  PASS
CM7 SPI loopback                   PASS
CM4 SPI loopback                   PASS
CM7 DMA/cache                      PASS
CM4 DMA/cache                      PASS
CM7 I2C controller/target          PASS
CM4 I2C controller/target          PASS
CM7 CMSIS/GPIO bring-up            PASS
CM7 Cortex-M startup/reset         PASS
CM7 GPIO pull-up                   PASS
CM7 GPIO pull-down                 PASS
CM4 CMSIS/GPIO bring-up            PASS
CM4 Cortex-M startup/reset         PASS
CM4 GPIO pull-up                   PASS
CM4 GPIO pull-down                 PASS
CM4 GPIO loopback low/high         PASS
CM4 GPIO open-drain                PASS
CM4 GPIO EXTI rising/falling       PASS
CM7 GPIO loopback low/high         PASS
CM7 GPIO open-drain                PASS
CM7 GPIO EXTI rising/falling       PASS
CM7 LED all off                    PASS
CM7 LED green only                 PASS
CM7 LED yellow only                PASS
CM7 LED red only                   PASS
CM7 LED all blink                  PASS
CM7 timer/PWM                      PASS
CM4 timer/PWM                      PASS
CM7 Ethernet Layer-2               PASS
```

## Evidence bundle

The campaign archive contains summary/metadata, build logs, OpenOCD logs, per-case GDB evidence, ELF/map files, symbol/size dumps, linker scripts and nested Ethernet build/traffic/debug evidence. Current Ethernet evidence additionally includes initial/down/recovered link-state GDB logs when the extended qualifier runs.

The accepted archive is:

```text
das-stm32h755-campaign-20260913T152845Z.tar.gz
```

The OpenOCD dual-core log contains repeated `Failed to read memory` diagnostics while OpenOCD probes STM32H7 flash/system regions during debugger connections. They did not correspond to failed acceptance cases: all 39 GDB/static cases passed, both core images executed, and the campaign exited 0. Treat these diagnostics as tooling noise unless they are accompanied by an acceptance failure or target bring-up failure.

The Ethernet flash log also reports OpenOCD adapter-speed fallback and an aligned extra erase range; programming and verification completed successfully.

## Recovery

Explicit destructive recovery remains separate:

```bash
./scripts/stm32h755_recover.sh
```

The normal campaign never performs an implicit mass erase.

## Qualification boundary

The recorded 39/39 baseline supports claims for the current linker/startup/vector model, clock/power, monotonic time, semantic board resources, GPIO/IRQ, polling UART, SPI, I2C, generic DMA/cache coherency, periodic timer/PWM on both cores where applicable, and connected CM7 polling Layer-2 Ethernet MAC/DMA/RMII/LAN8742A operation.

The current `develop` qualifier additionally implements cable link-loss/recovery checking, pending one physical rerun.

The baseline does not imply production dual-core lifecycle/HSEM/shared-memory coordination, Ethernet IRQ operation, IP networking, timer input capture, ADC, watchdog or internal-flash/reset-cause services.
