# Raw Ethernet example

This standalone installed-package consumer exercises the DAS Layer-2 Ethernet API on the NUCLEO-H755ZI-Q without lwIP or any IP configuration.

It is a **CM7 runtime example**. The source still cross-builds with the CM4 package so CI can validate the public contract, but the current STM32H755 board Ethernet backend deliberately returns `DAS_ERROR_UNSUPPORTED` on CM4 before changing RMII routing.

## Physical setup

```text
NUCLEO-H755ZI-Q connected through ST-LINK USB
JP6 fitted
JP7 fitted
CN14 RJ45  <->  Linux host Ethernet port
```

The automated scripts default to host interface `enp0s31f6`. No IPv4/IPv6 address, DHCP or network stack is required because qualification uses raw Ethernet frames.

## Firmware behavior

The application initializes:

```text
DAS_BOARD_ETH_RJ45
station MAC 02:00:00:00:00:01
```

while enabling the CM7 data cache. It polls PHY link state, transmits one 60-byte broadcast frame approximately once per second and continuously drains received frames.

Normal TX frame:

```text
destination: ff:ff:ff:ff:ff:ff
source:      02:00:00:00:00:01
EtherType:   0x88B5
payload:     DAS ETH L2 test
```

Qualification RX frames use EtherType `0x88B6` with a sequence number and deterministic payload pattern. Firmware validates those bytes and records accepted/error counts for batch GDB inspection.

## LEDs

```text
GREEN     PHY link up
YELLOW    toggles after each successful TX
RED       Ethernet/application error observed
```

## Debugger observables

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

## Build and flash only

```bash
./scripts/build_and_flash_eth_raw.sh /path/to/STM32CubeH7
```

This helper builds and installs DAS, builds this example only through `find_package(DAS)`, flashes CM7 and starts the firmware.

For manual host capture:

```bash
sudo tcpdump -i enp0s31f6 -e -XX 'ether proto 0x88b5'
```

## Automated qualification

Prefer the standalone qualifier when validating the backend:

```bash
./scripts/stm32h755_eth_test.sh /path/to/STM32CubeH7
```

Override the interface when needed:

```bash
./scripts/stm32h755_eth_test.sh /path/to/STM32CubeH7 --iface <linux-interface>
```

The qualifier is interactive only for the physical cable transition. It:

1. builds/flashes the installed-package example and verifies initial carrier/link state;
2. asks the operator to unplug CN14 (or the host end) and waits for host carrier loss;
3. reads the running STM32 through batch GDB and requires `g_das_eth_link_up == 0`, zero speed and unknown duplex;
4. asks the operator to reconnect the same cable and waits for carrier recovery;
5. verifies the same running Ethernet instance reports link-up again with negotiated 10/100 Mbps and half/full duplex, without reinitializing DAS Ethernet;
6. only after recovery, validates five STM32-to-host frames and injects 64 host-to-STM32 integrity frames;
7. finishes with the normal PHY/MAC/DMA/cache GDB evidence check.

The default operator timeout for each unplug/replug transition is 60 seconds. Override it with `DAS_ETH_LINK_TRANSITION_TIMEOUT=<seconds>` when necessary.

The accepted 2026-09-13 baseline negotiated 100 Mbps/full duplex, validated 5/5 TX frames and 64/64 RX integrity frames with zero errors and `DAS_OK`. That run predates the explicit unplug/replug phase. The connected raw-TX/RX baseline remains valid, while the extended qualifier must be rerun once before link-loss/recovery is considered physically qualified.

That same Ethernet qualifier remains acceptance point 39 in the standing STM32H755 campaign; extending the checks inside the case does not add another campaign acceptance point.

## Scope

This example proves raw Layer-2 operation through STM32H755 MAC/dedicated Ethernet DMA, RMII and the LAN8742A PHY with CM7 D-cache enabled.

It does not provide ARP, IP, UDP/TCP, DHCP, DNS, sockets, lwIP integration, Ethernet interrupt-driven operation or shared CM7/CM4 Ethernet ownership.
