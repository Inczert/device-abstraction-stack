# SPI

DAS exposes SPI controller transfers through an opaque generic handle. STM32 SPI instances, alternate functions, RCC mux fields, FIFO/status bits, DMA streams and DMAMUX request IDs stay below the public API.

## Controller API

```c
const das_spi_config_t config = {
    .frequency_hz = 4000000u,
    .mode = DAS_SPI_MODE_0,
    .bit_order = DAS_SPI_MSB_FIRST,
};

das_spi_t spi = DAS_SPI_INVALID;
(void)das_board_spi_init(DAS_BOARD_SPI_ARDUINO, &config, &spi);
```

The current baseline is 8-bit and supports modes 0..3 plus MSB/LSB-first operation. `frequency_hz` is a requested maximum; `das_spi_get_frequency()` returns the selected effective SCK.

## Polling transfers

```c
das_spi_transfer(spi, tx, rx, size);
das_spi_transfer_timeout(spi, tx, rx, size, timeout_ms);
```

SPI is full duplex. For polling convenience, `tx == NULL` sends `0xff` fill bytes and `rx == NULL` discards incoming bytes. Both cannot be null for a non-zero transfer.

## DMA transfers

Full-duplex SPI DMA is available:

```c
das_spi_transfer_dma(spi, tx, rx, size);
das_spi_transfer_dma_timeout(spi, tx, rx, size, timeout_ms);
```

The current DMA path requires both TX and RX buffers for non-zero transfers and uses implementation-selected DMA1/DMAMUX1 resources.

DMA does not make cacheable memory coherent automatically. On CM7, callers explicitly clean TX data and prepare/invalidate RX storage with `<das/cache.h>`. See [DMA and cache coherency](dma.md).

## Chip select

SPI transfer calls never assert/deassert chip select implicitly. For the Arduino route:

```c
das_board_spi_chip_select(DAS_BOARD_SPI_ARDUINO, true);
das_spi_transfer(spi, tx, rx, size);
das_board_spi_chip_select(DAS_BOARD_SPI_ARDUINO, false);
```

Applications with multiple devices can use additional normal DAS GPIOs as chip-select lines.

## NUCLEO-H755ZI-Q route

```text
DAS_BOARD_SPI_ARDUINO
SCK   PA5   SPI1_SCK   AF5
MISO  PA6   SPI1_MISO  AF5
MOSI  PB5   SPI1_MOSI  AF5
CS    PD14  GPIO, active low
```

## Clock policy

The STM32H755 backend derives/selects the SPI1 kernel clock from live RCC state and chooses the fastest supported prescaler that does not exceed the requested SCK. It does not assume SPI1 clock equals APB2.

## Hardware qualification

Polling fast-path qualifier:

```bash
./scripts/stm32h755_spi_test.sh /home/dev/STM32Cube/Repository/STM32CubeH7/
```

DMA qualifier:

```bash
./scripts/stm32h755_dma_test.sh /home/dev/STM32Cube/Repository/STM32CubeH7/
```

Both reuse:

```text
Arduino D11 / MOSI / PB5 <-> Arduino D12 / MISO / PA6
```

Polling qualification covers modes 0..3, both bit orders, 1/2/4/8 MHz requested SCK, transfer lengths 1/7/31/64, receive-only/transmit-only semantics and exact equality across 111 looped-back bytes on each core.

SPI-DMA qualification covers a 192-byte full-duplex physical transfer at 4 MHz on each core, with explicit CM7 cache maintenance.

Both polling and DMA SPI paths are part of the completed **38/38** standing STM32H755 campaign.
