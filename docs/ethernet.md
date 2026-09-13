# Ethernet

DAS exposes Ethernet as a Layer-2 API. IP networking deliberately remains outside the core DAS contract.

```text
application / optional network stack
        |
        v
DAS Layer-2 Ethernet API
        |
        v
STM32H755 ETH MAC + dedicated ETH DMA
        |
        v
RMII + LAN8742A PHY
        |
        v
CN14 RJ45
```

A higher stack such as lwIP may be integrated above DAS without introducing lwIP types into the public DAS API.

## Public API

Header: `<das/eth.h>`

```c
das_eth_t eth = DAS_ETH_INVALID;

das_eth_config_t config = {
    .mac = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01},
};

das_board_eth_init(DAS_BOARD_ETH_RJ45, &config, &eth);
das_eth_send(eth, frame, length);
das_eth_receive(eth, buffer, capacity, &received);
das_eth_link_state(eth, &state);
```

`das_eth_send()` accepts one complete Ethernet frame from destination MAC through payload, excluding FCS. The standard-frame ceiling is `DAS_ETH_MAX_FRAME_SIZE` (1518 bytes). DAS owns MAC CRC/padding, Ethernet-DMA descriptors and cache coherency.

`das_eth_receive()` is polling/non-blocking. No frame is represented by `DAS_OK` with `received == 0`. Available frames are copied whole into the caller buffer.

`das_eth_link_state()` reports link up/down plus negotiated speed and duplex. A down link reports zero speed and `DAS_ETH_DUPLEX_UNKNOWN`.

## STM32H755 / NUCLEO-H755ZI-Q backend

The current runtime backend is CM7-owned and polling-only. `DAS_BOARD_ETH_RJ45` configures the on-board RMII route to the LAN8742A using AF11:

```text
PA1   RMII_REF_CLK
PA2   RMII_MDIO
PC1   RMII_MDC
PA7   RMII_CRS_DV
PC4   RMII_RXD0
PC5   RMII_RXD1
PG11  RMII_TX_EN
PG13  RMII_TXD0
PB13  RMII_TXD1
```

The backend:

- enables/resets the STM32H755 Ethernet MAC/TX/RX clocks;
- selects RMII through SYSCFG;
- performs Ethernet-DMA software reset;
- discovers the LAN8742A over Clause-22 MDIO and starts auto-negotiation;
- synchronizes MAC speed/duplex from PHY state;
- programs the configured station MAC;
- uses four TX and four RX descriptors;
- uses one 1536-byte internal buffer per descriptor;
- performs polling raw-frame TX/RX;
- performs CM7 D-cache clean/invalidate ownership around descriptors and buffers.

Each software descriptor occupies one 32-byte CM7 cache line. The hardware descriptor stride is configured to match, so independently DMA-owned descriptors do not share one cache line.

The default CM7 linker places static writable data in AXI SRAM, which is accessible to Ethernet DMA. Custom linker scripts using Ethernet must likewise keep Ethernet descriptors/buffers in DMA-visible SRAM; DTCM is not suitable.

Ethernet owns its own DMA engine. It is **not** implemented through generic `das_dma_t` / DMA1 / DMAMUX1.

CM4 builds retain the public API for source compatibility, but `das_board_eth_init()` returns `DAS_ERROR_UNSUPPORTED` before changing RMII board routing. Shared CM7/CM4 Ethernet ownership is outside this baseline.

## Raw installed-package example

`examples/eth_raw` is an installed-package consumer using station MAC `02:00:00:00:00:01`. It broadcasts a 60-byte experimental EtherType `0x88B5` frame approximately once per second while continuously polling RX.

Runtime indicators:

```text
GREEN LED    PHY link up
YELLOW LED   toggles after successful TX
RED LED      Ethernet/application error observed
```

The qualifier also uses EtherType `0x88B6` host-to-board frames containing a sequence number and deterministic payload pattern. Firmware validates the payload and exposes counters for batch GDB inspection.

## Physical setup

```text
NUCLEO-H755ZI-Q connected through ST-LINK USB
JP6 fitted
JP7 fitted
CN14 RJ45 <-> Linux host Ethernet port
```

The scripts default to host interface `enp0s31f6`. Override with `--iface`, `--eth-iface`, or `DAS_ETH_IFACE` as appropriate. No IPv4/IPv6 address, DHCP or other network stack is required.

## Automated physical qualification

Run the focused qualifier with:

```bash
./scripts/stm32h755_eth_test.sh /path/to/STM32CubeH7
```

The qualifier exercises one firmware boot and one Ethernet initialization:

```text
initial carrier/link up
        |
        v
operator unplugs cable
        |
        v
host carrier down + STM32 link down
        |
        v
operator reconnects cable
        |
        v
host carrier up + STM32 negotiated link recovery
        |
        v
raw TX/RX integrity traffic after recovery
```

The script waits up to 60 seconds for each operator cable transition by default. `DAS_ETH_LINK_TRANSITION_TIMEOUT=<seconds>` overrides this timeout.

Linux `/sys/class/net/<iface>/carrier` proves the physical transition. Batch GDB then verifies the continuously running STM32 reports the corresponding DAS link state. GDB only halts/resumes for observation; the board is not reset and Ethernet is not reinitialized between link loss and recovery.

After recovery the qualifier requires:

- five valid STM32-to-host EtherType `0x88B5` frames;
- 64 host-to-STM32 EtherType `0x88B6` integrity frames;
- exact deterministic payload validation;
- zero RX integrity errors;
- valid PHY speed/duplex;
- `DAS_OK` final result;
- continued descriptor recycling with CM7 D-cache enabled.

## Qualified results

The standing full STM32H755 campaign passed **39/39** on 2026-09-13 at DAS commit `f6b65672d9ae69cf28cd574d0dbba01cf875d8dc` using STM32CubeH7 `f5c0b7a2b1f6eb26fde150f72edb2d7deb647066`.

```text
PASS: 39
FAIL: 0
Exit code: 0
Evidence archive: das-stm32h755-campaign-20260913T152845Z.tar.gz
```

That full-campaign archive established the connected Ethernet baseline.

The extended unplug/replug qualifier was subsequently physically confirmed on 2026-09-13 with recovery-test implementation commit `9a8b628dcea8ad724282103659a721066efcd9dd`:

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

This qualifies link-down reporting, negotiated link recovery on the same initialized Ethernet instance, and resumed bidirectional raw traffic after recovery.

The full campaign invokes this focused qualifier as acceptance point 39. Strengthening the internal Ethernet checks does not create an artificial 40th campaign case.

## Qualified boundary

Qualified:

- STM32H755 ETH MAC and dedicated DMA;
- NUCLEO RMII routing;
- LAN8742A MDIO discovery and auto-negotiation;
- connected and disconnected link-state reporting;
- cable replug and negotiated recovery without Ethernet reinitialization;
- post-recovery polling raw TX/RX;
- descriptor recycling;
- CM7 D-cache coherency;
- installed-package consumer integration.

Not part of this baseline:

- Ethernet IRQ-driven operation;
- lwIP;
- ARP/IP/ICMP/DHCP;
- UDP/TCP/DNS/socket APIs;
- shared CM7/CM4 Ethernet ownership.
