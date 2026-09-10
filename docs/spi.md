# SPI

DAS exposes SPI controller transfers through an opaque generic handle. Application code does not select STM32 SPI instances, alternate functions, RCC mux values, FIFO/status bits or baud-divider encodings.

## Controller API

The current baseline is byte-oriented and supports all four conventional SPI modes plus both bit orders:

```c
const das_spi_config_t config = {
    .frequency_hz = 4000000u,
    .mode = DAS_SPI_MODE_0,
    .bit_order = DAS_SPI_MSB_FIRST,
};

das_spi_t spi = DAS_SPI_INVALID;
if (das_board_spi_init(DAS_BOARD_SPI_ARDUINO, &config, &spi) != DAS_OK) {
    /* handle error */
}
```

`frequency_hz` is a requested **maximum** serial-clock rate. The backend chooses the fastest supported divider that does not exceed it. `das_spi_get_frequency()` returns the effective SCK rate.

The STM32H755 baseline uses eight application bits per frame. Wider frames can be added later if a portable use case requires them rather than leaking every STM32 DSIZE possibility into the first API.

## Transfers

SPI is inherently full duplex. The generic transfer call therefore accepts independent TX and RX buffers:

```c
das_spi_transfer(spi, tx, rx, size);
```

For asymmetric application use:

- `tx == NULL` transmits `0xff` fill bytes while receiving;
- `rx == NULL` discards received bytes while transmitting;
- both may not be `NULL` for a non-zero transfer.

`das_spi_transfer_timeout()` uses the generic DAS monotonic time source for a finite timeout. `das_spi_transfer()` blocks until completion.

## Chip select ownership

A transfer **does not assert or deassert chip select automatically**. Chip select belongs to the transaction/device policy above the controller because real SPI devices often require several transfers under one assertion.

For the NUCLEO semantic Arduino route, DAS provides one convenient board default:

```c
das_board_spi_chip_select(DAS_BOARD_SPI_ARDUINO, true);  /* active low */
das_spi_transfer(spi, tx, rx, size);
das_board_spi_chip_select(DAS_BOARD_SPI_ARDUINO, false);
```

Applications with multiple devices can use any normal DAS GPIO as additional chip-select lines. The controller API itself is intentionally unaware of them.

## NUCLEO-H755ZI-Q route

The semantic resource maps internally to:

```text
DAS_BOARD_SPI_ARDUINO
SCK   PA5   SPI1 SCK   AF5
MISO  PA6   SPI1 MISO  AF5
MOSI  PB5   SPI1 MOSI  AF5
CS    PD14  GPIO, active low
```

These STM32 details remain in the board/device layers.

## STM32H755 clock policy

SPI1 belongs to the SPI1/2/3 kernel-clock group. The current backend selects `PER_CK` for that group and selects live HSI as `PER_CK`, then derives the actual HSI rate from the RCC HSI divider before choosing the SPI master prescaler.

This makes the serial clock explicit and independent of whether CM7 is currently using the 64, 200, 300 or 400 MHz board profile. It also avoids pretending that the SPI1 kernel clock is simply APB2.

The first baseline supports master/controller mode only. Interrupt-driven and DMA transfer paths are deliberately deferred; DMA belongs with #9 and should reuse this framing/clock/board route rather than replacing it.

## Focused physical qualification

Before SPI is promoted into the standing campaign, run:

```bash
./scripts/stm32h755_spi_test.sh /home/dev/STM32Cube/Repository/STM32CubeH7/
```

Connect one jumper:

```text
Arduino SPI MOSI / PB5 ---- jumper ---- Arduino SPI MISO / PA6
```

Leave SCK/PA5 and CS/PD14 otherwise unconnected. The qualifier runs separate CM7 and CM4 images and checks:

- semantic PA5/PA6/PB5/PD14 board mapping;
- active-low default CS helper;
- mode 0, 1, 2 and 3 transfers;
- MSB-first and LSB-first operation;
- effective 1, 2, 4 and 8 MHz serial clocks;
- physical MOSI-to-MISO equality over transfer lengths 1, 7, 31 and 64 bytes;
- receive-only fill semantics and transmit-only discard semantics;
- exact equality of 111 physically looped-back bytes;
- continued execution after all transfers.

CM7 first selects the qualified 400 MHz board profile. CM4 runs after reset and independently exercises its peripheral-clock-enable view.

Once both focused cases pass, they are added to the main hardware campaign. The MOSI/MISO jumper can then become another persistent initial fixture, avoiding yet another mid-campaign cable ritual.
