# Hardware qualification

DAS treats physical target testing as part of backend qualification. The STM32H755 campaign combines static image/linker checks with execution on the real NUCLEO-H755ZI-Q and packages evidence into timestamped archives.

Current target:

```text
Board:    NUCLEO-H755ZI-Q
Device:   STM32H755
CPU1:     Cortex-M7
CPU2:     Cortex-M4
Debug:    ST-LINK direct DAP + OpenOCD + GDB
```

Debugger-driven CM4 execution proves the CM4 image and supported device paths on real CPU2 silicon. It does not yet prove the production CM7-to-CM4 boot/release, HSEM or shared-memory ownership model tracked separately.

## Standing regression baseline

The latest completed full campaign is **40/40 PASS**:

```text
DAS commit:       737863075dfed78f73f99ae58f6e0ca52ffea93e
UTC start:        2026-10-04T13:42:17Z
STM32CubeH7:      f5c0b7a2b1f6eb26fde150f72edb2d7deb647066
PASS:             40
FAIL:             0
Exit code:        0
Evidence archive: das-stm32h755-campaign-20261004T134217Z.tar.gz
```

That campaign covers linker/layout, startup/vector ownership, CM7+CM4 hard-float startup with real VFP arithmetic, time, clock/power, semantic board resources, GPIO/EXTI/IRQ, UART, SPI, I2C, generic DMA/cache, timer/PWM and CM7 Layer-2 Ethernet.

The dedicated hard-float case verified real VFP arithmetic is present in both ELFs, CP10/CP11 are fully enabled before application execution, CFSR reports no NOCP fault, and `1.5f * 2.25f + 0.5f` produces exact result bits `0x40780000` on both physical cores.

The same 2026-10-04 run also revalidated the complete Ethernet unplug/replug path and post-recovery bidirectional traffic.

## Testing model

Peripheral development uses two physical-test levels:

1. focused qualifier during implementation/debugging;
2. promotion of the same acceptance path into the full campaign once the focused path passes.

Focused scripts include:

```bash
./scripts/stm32h755_uart_test.sh  /path/to/STM32CubeH7
./scripts/stm32h755_timer_test.sh /path/to/STM32CubeH7
./scripts/stm32h755_spi_test.sh   /path/to/STM32CubeH7
./scripts/stm32h755_i2c_test.sh   /path/to/STM32CubeH7
./scripts/stm32h755_dma_test.sh   /path/to/STM32CubeH7
./scripts/stm32h755_fpu_test.sh   /path/to/STM32CubeH7
./scripts/stm32h755_eth_test.sh   /path/to/STM32CubeH7
```

Ethernet defaults to Linux interface `enp0s31f6`. Use `--iface` for the focused qualifier, `--eth-iface` for the full campaign, or `DAS_ETH_IFACE` for either when the host interface differs.

## Running the full campaign

Run the campaign from a clean checkout of the final `develop` release candidate. Check `git status --short` before running, and preserve the commit SHA printed in the resulting evidence archive. For the v0.1.0 baseline, use STM32CubeH7 commit `f5c0b7a2b1f6eb26fde150f72edb2d7deb647066`.

```bash
git status --short
git rev-parse HEAD
git -C /home/dev/STM32Cube/Repository/STM32CubeH7/ rev-parse HEAD

./scripts/stm32h755_test_campaign.sh \
  /home/dev/STM32Cube/Repository/STM32CubeH7/ \
  --clean \
  --eth-iface <linux-interface>
```

Select the Linux NIC physically wired to board CN14; the interface default `enp0s31f6` is only the previously qualified host's name. The script checks that the supplied NIC exists before starting the campaign.

Evidence directories and archives are stored under:

```text
build/stm32h755/campaign/
```

An archive is also produced after a logged failure.

## Initial hardware setup

Install the persistent fixtures before starting:

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
jumper E: keep ready for later D4 <-> D3 transition
B1 USER: released

Ethernet:
JP6 fitted
JP7 fitted
board RJ45 CN14 <-> host PC Ethernet port
host interface defaults to enp0s31f6
keep the cable connected initially; the Ethernet qualifier will prompt for one unplug/replug cycle
```

The Ethernet case uses raw Layer-2 frames and needs no IPv4/IPv6 address, DHCP or other network stack.

Leave SPI D13/SCK/PA5 and D10/CS/PD14 otherwise unconnected. Never connect loopback signal pins to 3V3, 5V or GND.

After the D3-free pull tests, the campaign asks once for:

```text
CN10 D4 / PE14 <-> CN10 D3 / PE13
```

That jumper remains connected for GPIO loopback/open-drain/EXTI and timer/PWM tests on both cores.

## Acceptance scope

### Static/linker

```text
STM32H755 CM7 memory layout
STM32H755 CM4 memory layout
Custom linker override
```

Expected defaults:

```text
CM7 vector      0x08000000
CM7 runtime RAM AXI SRAM, stack top 0x24080000
CM4 vector      0x08100000
CM4 runtime RAM D2 SRAM1, stack top 0x30020000
custom CM7      vector at 0x08020000
```

### Core/startup, hard-float and time

OpenOCD exposes CM7 on GDB port 3333 and CM4 on 3334. Both cores qualify their image/startup path and monotonic-time behavior.

The focused hard-float qualifier builds a dedicated image for each core, verifies the ELF contains a real single-precision VFP arithmetic instruction, then checks on silicon that startup granted full CP10/CP11 access before that instruction executes. The firmware computes `1.5f * 2.25f + 0.5f`, requires the exact IEEE-754 result bits `0x40780000` (3.875), and rejects any NOCP UsageFault evidence. CM7 uses `fpv5-d16`; CM4 uses `fpv4-sp-d16`.

No external signal wiring is needed for this case beyond the ST-LINK USB connection.

Time checks include source injection, wrap-safe helpers, SysTick setup from the live clock and measured delay behavior.

### Clock/power

CM7 qualifies the supported 64/200/300/400 MHz board profiles, rejection of unsupported 480 MHz, regulator/voltage readiness and final live clock-tree readback.

### Board button and GPIO/IRQ

B1 USER qualifies polling plus EXTI. CM7 and CM4 qualify pull-up/down, physical D4-to-D3 low/high loopback, open-drain behavior and rising/falling EXTI, including generic IRQ enable/priority/pending behavior.

CM7 additionally performs the manual visual LED sequence: all off, green only, yellow only, red only, then all blinking.

### UART

Fixture:

```text
D1 / PB6 TX <-> D0 / PB7 RX
```

Both cores qualify finite timeout, semantic routing, live baud generation, 115200 8N1, 57600 8E2, 38400 7O1 and exact deterministic loopback data.

### SPI

Fixture:

```text
D11 / PB5 MOSI <-> D12 / PA6 MISO
```

Both cores qualify modes 0..3, both bit orders, multiple requested SCK rates and deterministic polling loopback data.

### DMA/cache

The DMA case reuses the SPI fixture. Both cores qualify generic DMA allocation/configuration/release, IRQ resolution, memory-to-memory integrity and a full-duplex SPI DMA transfer. CM7 additionally qualifies the explicit D-cache maintenance contract; CM4 qualifies the same public API with expected no-D-cache behavior.

Generic `das_dma_t` uses DMA1/DMAMUX1. Ethernet uses the Ethernet peripheral's independent descriptor-based DMA engine.

### I2C

Fixture:

```text
D15 / PB8 / I2C1_SCL <-> D69 / PF14 / I2C4_SCL
D14 / PB9 / I2C1_SDA <-> D68 / PF15 / I2C4_SDA
                                target 0x52
```

Each core qualifies 100/400 kHz timing, probe/NACK behavior, physical write/read, repeated-START operation and deterministic payload integrity.

### Timer/PWM

With D4 connected to D3, both cores qualify a 1 kHz periodic timer, start/stop/counter behavior, TIM2 update delivery and physical 1 kHz PWM at 25/50/75% duty.

### Ethernet Layer 2

Ethernet runtime ownership is CM7-only. The full campaign ends its long-lived dual-core OpenOCD session and invokes the standalone Ethernet qualifier.

Fixture:

```text
NUCLEO-H755ZI-Q CN14 RJ45 <-> host PC Ethernet port
JP6 fitted
JP7 fitted
```

The qualified sequence is:

```text
initial carrier + STM32 link up
        -> unplug cable
host carrier down + STM32 link down
        -> reconnect cable
host carrier up + STM32 negotiated recovery
        -> post-recovery raw TX/RX integrity traffic
```

The 2026-09-13 recovery run proved:

```text
initial STM32 link:              up, 100 Mbps, full duplex
host carrier after unplug:      down
STM32 after unplug:             up=0, speed=0, duplex=UNKNOWN, DAS_OK
host carrier after replug:      up
STM32 after replug:             up=1, 100 Mbps, full duplex, DAS_OK
Ethernet reinitialization:      none
STM32 -> host after recovery:   5/5 valid frames
host -> STM32 after recovery:   64/64 integrity frames
RX integrity errors:            0
last sequence:                  64
final TX count:                 17
final RX count / bytes:         79 / 6354
final DAS result:               DAS_OK
final GDB result:               PASS
```

STM32-to-host uses broadcast EtherType `0x88B5` frames from MAC `02:00:00:00:00:01`. Host-to-STM32 qualification uses EtherType `0x88B6`, sequence numbers and deterministic payload data validated by firmware.

This qualifies RMII routing, LAN8742A/MDIO link management, MAC configuration, link-loss reporting, renegotiated recovery without Ethernet reinitialization, TX/RX descriptor recycling, post-recovery raw frame transfer and CM7 D-cache coherency.

## 40-case acceptance summary

The list below is the physically completed 2026-10-04 campaign.

```text
STM32H755 CM7 memory layout        PASS
STM32H755 CM4 memory layout        PASS
Custom linker override             PASS
Cortex-M hard-float startup        PASS
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

The Ethernet acceptance case includes the qualified unplug/replug/recovery checks internally. The hard-float startup qualifier covers both CM7 and CM4 in one focused acceptance case.

## Evidence bundle

Campaign archives contain summary/metadata, build logs, OpenOCD logs, per-case GDB evidence, ELF/map files, symbol/size dumps, linker scripts, nested hard-float evidence and nested Ethernet evidence. The current Ethernet qualifier additionally records initial/down/recovered link-state GDB logs plus host traffic and final state logs.

The accepted full-campaign archive is:

```text
das-stm32h755-campaign-20261004T134217Z.tar.gz
```

The archive contains the full 40-case evidence bundle, including nested hard-float and Ethernet qualification logs, and exited with zero failures.

## Recovery

Explicit destructive recovery remains separate:

```bash
./scripts/stm32h755_recover.sh
```

The normal campaign never performs an implicit mass erase.

## Qualification boundary

The qualified baseline covers the current linker/startup/vector model, hard-float CP10/CP11 enablement and real VFP execution on both cores, clock/power, monotonic time, semantic board resources, GPIO/IRQ, polling UART, SPI, I2C, generic DMA/cache coherency, periodic timer/PWM on both cores where applicable, and CM7 polling Layer-2 Ethernet including link loss/recovery and post-recovery traffic.

It does not imply production dual-core lifecycle/HSEM/shared-memory coordination, Ethernet IRQ-driven operation, IP networking, timer input capture, ADC, watchdog or internal-flash/reset-cause services.


## Release qualification gate

The full hardware campaign is the manual release authority. Before a release candidate is promoted from `develop` to `main`, it must finish with exactly 40 passes, zero failures and exit code zero. The campaign writes a SHA-256 alongside its evidence archive; preserve both as release evidence.

Release automation begins only after promotion. Successful `main` CI reads the root `VERSION` file, creates `v<VERSION>` when that tag does not already exist, and launches packaging automatically.

The release workflow intentionally does not try to infer whether local hardware testing happened. If source content changes after the successful campaign, rerun the complete campaign before merging/pushing to `main`.
