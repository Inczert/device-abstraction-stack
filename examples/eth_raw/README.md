# Raw Ethernet example

This installed-package consumer exercises the DAS Layer-2 Ethernet API on the NUCLEO-H755ZI-Q without lwIP or IP configuration.

It is a **CM7 runtime example**. The source still cross-builds with the CM4 package so CI can validate the public API, but the STM32H755 board backend deliberately returns `DAS_ERROR_UNSUPPORTED` on CM4 before changing RMII routing.

## Physical setup

```text
NUCLEO-H755ZI-Q connected through ST-LINK USB
JP6 fitted
JP7 fitted
CN14 RJ45 <-> Linux host Ethernet port
```

The automation defaults to host interface `enp0s31f6`. No IPv4/IPv6 address, DHCP or network stack is required.

## Firmware behavior

The application initializes:

```text
DAS_BOARD_ETH_RJ45
station MAC 02:00:00:00:00:01
```

with CM7 D-cache enabled. It polls PHY link state, transmits one 60-byte broadcast frame approximately once per second, and continuously drains received frames.

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

The helper builds and installs DAS, builds this example through `find_package(DAS)`, flashes CM7, and starts the firmware.

For manual host capture:

```bash
sudo tcpdump -i enp0s31f6 -e -XX 'ether proto 0x88b5'
```

## Automated qualification

Prefer the standalone qualifier:

```bash
./scripts/stm32h755_eth_test.sh /path/to/STM32CubeH7
```

Override the interface when necessary:

```bash
./scripts/stm32h755_eth_test.sh /path/to/STM32CubeH7 --iface <linux-interface>
```

The qualifier is interactive only for the physical cable transition. It:

1. builds/flashes the installed-package example and verifies initial carrier/link state;
2. asks the operator to unplug CN14 (or the host end) and waits for carrier loss;
3. reads the running STM32 through batch GDB and requires link-down, zero speed and unknown duplex;
4. asks the operator to reconnect the same cable and waits for carrier recovery;
5. verifies the same initialized Ethernet instance negotiates link-up again without reinitialization;
6. after recovery, validates five STM32-to-host frames and injects 64 host-to-STM32 integrity frames;
7. finishes with PHY/MAC/DMA/cache GDB evidence.

The default operator timeout for each unplug/replug transition is 60 seconds. Override it with `DAS_ETH_LINK_TRANSITION_TIMEOUT=<seconds>`.

## Qualified result

The connected Ethernet case is acceptance point 39 in the standing **39/39 PASS** STM32H755 campaign from 2026-09-13.

The extended unplug/replug qualifier was then physically confirmed on the same day with recovery-test implementation commit `9a8b628dcea8ad724282103659a721066efcd9dd`:

```text
initial link:                    100 Mbps / full duplex
unplug host carrier:            PASS
STM32 link-down:                PASS
replug host carrier:            PASS
STM32 recovery without reinit:  PASS, 100 Mbps / full duplex
STM32 -> host after recovery:   5/5 PASS
host -> STM32 after recovery:   64/64 PASS
RX integrity errors:            0
final TX count:                 17
final RX count / bytes:         79 / 6354
final DAS result:               DAS_OK
final GDB result:               PASS
```

The test therefore proves not only that the PHY renegotiates, but that raw traffic resumes through the same MAC/DMA/cache state after recovery.

## Scope

This example qualifies raw Layer-2 operation through STM32H755 MAC/dedicated Ethernet DMA, RMII and LAN8742A with CM7 D-cache enabled, including cable loss/recovery.

It does not provide ARP, IP, UDP/TCP, DHCP, DNS, sockets, lwIP integration, Ethernet interrupt-driven operation or shared CM7/CM4 Ethernet ownership.
