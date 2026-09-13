# Ethernet

DAS exposes Ethernet as a Layer-2 device API. IP networking remains outside the core DAS contract.

```text
application / network stack
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
RJ45
```

A higher stack such as lwIP may be integrated above DAS later without introducing lwIP types into the public DAS API.

## Public API

Header: `<das/eth.h>`

```c
das_eth_t eth = DAS_ETH_INVALID;

das_eth_config_t config = {
    .mac = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01},
};

das_result_t result = das_board_eth_init(
    DAS_BOARD_ETH_RJ45,
    &config,
    &eth);
```

Raw Layer-2 operations are:

```c
das_eth_send(eth, frame, length);

das_eth_receive(eth, buffer, capacity, &received);

das_eth_link_state_t state;
das_eth_link_state(eth, &state);
```

`das_eth_send()` accepts one complete Ethernet frame from destination MAC through payload, excluding Ethernet FCS. The current standard-frame ceiling is `DAS_ETH_MAX_FRAME_SIZE` (1518 bytes). DAS owns MAC CRC/padding, Ethernet-DMA descriptors and cache coherency.

`das_eth_receive()` is polling/non-blocking. No available frame is represented by `DAS_OK` with `received == 0`. An available frame is copied as one complete Layer-2 frame into the caller buffer.

`das_eth_link_state()` reports link up/down plus negotiated speed and duplex. A down link reports zero speed and `DAS_ETH_DUPLEX_UNKNOWN`.

## STM32H755 / NUCLEO-H755ZI-Q backend

The current backend is deliberately CM7-owned and polling-only. `DAS_BOARD_ETH_RJ45` configures the on-board RMII route to the LAN8742A using AF11:

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
- performs Ethernet DMA software reset;
- discovers the LAN8742A over Clause-22 MDIO and restarts auto-negotiation;
- synchronizes MAC speed/duplex from PHY state;
- programs the configured station MAC;
- uses four TX and four RX descriptors;
- uses one 1536-byte internal frame buffer per descriptor;
- performs polling raw-frame TX/RX;
- performs CM7 D-cache maintenance around descriptors and buffers.

Each software descriptor occupies one 32-byte CM7 cache line. The STM32H755 descriptor-skip field is configured so hardware uses the same stride, preventing independent DMA-owned descriptors from sharing a cache line.

The default CM7 linker places static writable data in AXI SRAM, which is Ethernet-DMA accessible. Custom linker scripts using Ethernet must likewise place descriptors/buffers in Ethernet-DMA-visible SRAM; DTCM is not suitable.

The Ethernet peripheral owns its own DMA engine. It is **not** implemented through the generic `das_dma_t` DMA1/DMAMUX1 API.

CM4 builds retain the API for source compatibility, but `das_board_eth_init()` returns `DAS_ERROR_UNSUPPORTED` before changing RMII board routing. Shared/dual-core Ethernet ownership is outside this baseline.

## Raw installed-package example

`examples/eth_raw` is an installed-package consumer using station MAC `02:00:00:00:00:01`. It broadcasts a 60-byte experimental EtherType `0x88B5` frame approximately once per second while polling RX.

Runtime indicators:

```text
GREEN LED    PHY link up
YELLOW LED   toggles after each successful TX
RED LED      error observed
```

Debugger evidence variables include:

```text
g_das_eth_link_up
g_das_eth_speed_mbps
g_das_eth_duplex
g_das_eth_tx_count
g_das_eth_rx_count
g_das_eth_rx_bytes
g_das_eth_rx_test_count
g_das_eth_rx_test_errors
g_das_eth_rx_last_sequence
g_das_eth_last_result
```

Build/install/flash only:

```bash
./scripts/build_and_flash_eth_raw.sh /path/to/STM32CubeH7
```

## Automated physical qualification

Physical setup:

```text
JP6 fitted
JP7 fitted
NUCLEO-H755ZI-Q CN14 RJ45  <->  host PC Ethernet port
```

The focused test defaults to Linux interface `enp0s31f6`:

```bash
./scripts/stm32h755_eth_test.sh /path/to/STM32CubeH7
```

Override with `--iface <linux-interface>` or `DAS_ETH_IFACE=<linux-interface>` when necessary.

No IP address, DHCP or higher network stack is required. The host uses Linux raw Layer-2 sockets.

The qualifier checks:

- physical carrier;
- PHY link state, speed and duplex;
- five valid STM32-to-host EtherType `0x88B5` frames;
- 64 host-to-STM32 EtherType `0x88B6` integrity frames;
- monotonic RX sequence tracking and deterministic payload validation;
- zero firmware integrity errors;
- repeated TX/RX descriptor recycling;
- CM7 D-cache coherency;
- installed-package consumption through `examples/eth_raw`.

## Qualified result

Ethernet is part of the standing **39/39 PASS** STM32H755 campaign at DAS commit `f6b65672d9ae69cf28cd574d0dbba01cf875d8dc`, run on 2026-09-13.

The recorded Ethernet evidence was:

```text
Host interface: enp0s31f6
Physical carrier: PASS
PHY: 100 Mbps / full duplex
STM32 -> host: 5/5 validated frames
TX interval: 0.995806 .. 0.995980 s
Host -> STM32: 64 frames injected
Firmware test_count: 64
test_errors: 0
last_sequence: 64
last_result: DAS_OK
RESULT: PASS
```

The firmware had transmitted eight frames by the debugger snapshot and had received 66 total Ethernet frames / 4229 bytes, of which all 64 dedicated integrity frames were accepted without error.

The full campaign uses the same focused qualifier as acceptance point 39 and archives its build/flash, host-traffic, OpenOCD, GDB and ELF/symbol evidence.

An explicit cable-disconnected/down-link transition was not exercised in the recorded campaign. That remains a narrower follow-up validation item and does not change the qualified connected raw-TX/RX baseline.

## Current boundary

Qualified:

- STM32H755 ETH MAC and dedicated DMA;
- RMII routing;
- LAN8742A MDIO discovery/auto-negotiation;
- connected link-state reporting;
- polling raw TX/RX;
- repeated descriptor recycling;
- CM7 D-cache coherency;
- installed-package consumer integration.

Not part of this baseline:

- Ethernet IRQ-driven operation;
- lwIP;
- ARP/IP/ICMP/DHCP;
- UDP/TCP/DNS/socket APIs;
- shared CM7/CM4 Ethernet ownership.
