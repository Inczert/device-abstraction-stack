# Ethernet

DAS exposes Ethernet as a Layer-2 device API. IP networking remains outside the core DAS contract.

The intended stack boundary is:

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

`das_eth_send()` accepts one complete Ethernet frame supplied by the caller. DAS owns any backend-specific MAC, DMA-descriptor and cache-coherency work.

`das_eth_receive()` is polling/non-blocking at this API level. A successful call with no currently available frame returns `DAS_OK` with `received == 0`. Once an RX frame is available the backend copies one complete frame into the supplied buffer and reports its byte count.

`das_eth_link_state()` reports whether the PHY link is up plus its negotiated speed and duplex mode. A down link uses `speed_mbps == 0` and `DAS_ETH_DUPLEX_UNKNOWN`.

## Current implementation boundary

The public API, opaque handle model and `DAS_BOARD_ETH_RJ45` semantic board resource are present. The STM32H755 backend currently returns `DAS_ERROR_UNSUPPORTED` for initialization and data-path operations.

This is deliberate. A real STM32H755 Ethernet implementation requires the MAC DMA descriptor rings, RX/TX buffers, RMII/MDIO setup, PHY management and CM7 cache-coherency rules. The API is being stabilized first rather than exposing a partially working hardware path.

The next backend step is to implement and physically qualify that path before Ethernet is listed as a supported DAS board feature.
