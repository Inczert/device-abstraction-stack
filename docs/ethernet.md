# Ethernet

DAS exposes Ethernet as a Layer-2 device API. IP networking remains outside the core DAS contract.

The stack boundary is:

```text
application / network stack
        |
        v
DAS Layer-2 Ethernet API
        |
        v
STM32H755 ETH MAC + DMA
        |
        v
RMII + LAN8742A PHY
        |
        v
RJ45
```

Protocol stacks such as lwIP may be integrated above DAS later without introducing lwIP types into the public DAS API.

## Public API

The application-facing configuration contains only the station MAC address:

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

The low-level transport remains Layer 2:

```c
das_eth_send(eth, frame, length);

das_eth_receive(eth, buffer, capacity, &received);

das_eth_link_state_t state;
das_eth_link_state(eth, &state);
```

`das_eth_send()` accepts one complete Ethernet frame supplied by the caller, excluding the Ethernet FCS. The current standard-frame ceiling is `DAS_ETH_MAX_FRAME_SIZE` (1518 bytes). DAS owns MAC CRC/padding, DMA descriptors and cache coherency.

`das_eth_receive()` is polling/non-blocking. A successful call with no currently available frame returns `DAS_OK` with `received == 0`. Once an RX frame is available the backend copies one complete frame into the supplied buffer and reports its byte count.

`das_eth_link_state()` reports whether the PHY link is up plus its negotiated speed and duplex mode. A down link uses `speed_mbps == 0` and `DAS_ETH_DUPLEX_UNKNOWN`.

## NUCLEO-H755ZI-Q implementation

The initial hardware backend is deliberately CM7-owned and polling-only. `DAS_BOARD_ETH_RJ45` configures the on-board RMII route to the LAN8742A using AF11:

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
- selects RMII in SYSCFG;
- performs the MAC DMA software reset;
- discovers the LAN8742A PHY over Clause-22 MDIO and restarts auto-negotiation;
- synchronizes MAC speed/duplex from LAN8742A link state;
- uses four TX and four RX descriptors;
- uses one 1536-byte internal buffer per descriptor;
- performs raw one-frame TX/RX without interrupts;
- owns CM7 D-cache clean/invalidate operations around DMA-visible descriptors and buffers.

Each software descriptor occupies one 32-byte CM7 cache line. The STM32H755 descriptor-skip field is configured so the hardware descriptor stride matches that layout, preventing separate DMA-owned descriptors from sharing a cache line.

The DAS default CM7 linker places static data in AXI SRAM, which is used for the descriptor and buffer arrays. A custom linker used with this backend must likewise place those objects in SRAM accessible by the Ethernet DMA; DTCM is not an appropriate placement for Ethernet DMA data.

CM4 builds keep the API available for source compatibility but `das_board_eth_init()` returns `DAS_ERROR_UNSUPPORTED` before configuring the RMII pins. Shared/dual-core Ethernet ownership is intentionally outside this baseline.

## Raw hardware example

`examples/eth_raw` is an installed-package consumer for physical Layer-2 bring-up. It uses MAC address `02:00:00:00:00:01` and broadcasts a 60-byte frame with experimental EtherType `0x88B5` once per second while polling RX.

Runtime indicators:

```text
GREEN LED    PHY link up
YELLOW LED   toggles after each successful TX
RED LED      error observed
```

Debugger observables:

```text
g_das_eth_link_up
g_das_eth_speed_mbps
g_das_eth_duplex
g_das_eth_tx_count
g_das_eth_rx_count
g_das_eth_rx_bytes
g_das_eth_last_result
```

Build/install/flash it with:

```bash
./scripts/build_and_flash_eth_raw.sh /path/to/STM32CubeH7
```

Then capture the board's frames on the connected Linux Ethernet interface:

```bash
sudo tcpdump -i <iface> -e -XX 'ether proto 0x88b5'
```

The expected source is `02:00:00:00:00:01` and the destination is broadcast. The NUCLEO Ethernet route also requires its board jumpers/solder configuration to be in the Ethernet position, including JP6 and JP7 fitted on the standard NUCLEO-H755ZI-Q setup.

## Qualification status

The public headers and STM32H755 implementation cross-build for CM7 and CM4; the CM4 runtime path is explicitly unsupported. The backend is not yet part of the standing physical qualification baseline. Physical acceptance requires observing PHY link state plus captured raw TX and RX traffic with CM7 D-cache enabled before Ethernet is marked qualified.
