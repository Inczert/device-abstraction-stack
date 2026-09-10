# UART

DAS exposes UART as a generic handle plus configuration/transfer API. Applications do not use `USART_TypeDef`, STM32 instance numbers, alternate-function selectors, RCC mux fields, or BRR calculations.

## Board-oriented setup

For the NUCLEO-H755ZI-Q, applications normally select a semantic board route:

```c
const das_uart_config_t config = {
    .baud_rate = 115200u,
    .data_bits = DAS_UART_DATA_BITS_8,
    .parity = DAS_UART_PARITY_NONE,
    .stop_bits = DAS_UART_STOP_BITS_1,
};

das_uart_t uart = DAS_UART_INVALID;
if (das_board_uart_init(DAS_BOARD_UART_STLINK_VCP, &config, &uart) != DAS_OK) {
    /* handle failure */
}
```

The board layer resolves the route and configures its GPIO alternate functions. The STM32H755 device backend then configures the selected USART and returns only an opaque `das_uart_t` to the caller.

Current board routes are:

| DAS resource | Physical route | STM32 backend fact |
| --- | --- | --- |
| `DAS_BOARD_UART_STLINK_VCP` | PD8 TX / PD9 RX | USART3, AF7 |
| `DAS_BOARD_UART_ARDUINO` | PB6 TX / PB7 RX | USART1, AF7 |

The final column is documentation for porting/debugging. Application code should not depend on it.

## Supported framing

The current generic API supports 7 or 8 application data bits, optional even/odd parity, and one or two stop bits. `data_bits` describes application data bits and excludes the parity bit. The STM32 backend maps that contract onto the device word-length fields.

Examples:

```text
8N1 -> 8 data bits, no parity, 1 stop bit
8E2 -> 8 data bits, even parity, 2 stop bits
7O1 -> 7 data bits, odd parity, 1 stop bit
```

A parity bit is never application data. The STM32H755 backend derives the application-data mask from the live M0/M1/PCE configuration and masks transmit/receive register values accordingly. For a 7-bit configuration, only bits 0..6 are exposed through the byte API even if the hardware receive register contains the parity position in bit 7.

Nine application data bits are not exposed by this byte-oriented API. Supporting that cleanly requires a 16-bit data path rather than quietly truncating bit 8.

## Clocking and baud rate

For the currently supported USART1/USART3 routes, the STM32H755 backend selects the corresponding APB peripheral clock as the USART kernel clock. It obtains the live APB frequency from the DAS STM32H755 clock engine and computes BRR using oversampling by 16.

This means the application provides only the requested baud rate. It does not calculate divisors and does not assume that the CPU frequency equals the UART peripheral clock.

`das_uart_get_baud_rate()` reports the effective baud rate represented by the live peripheral clock and BRR value.

## Blocking I/O and timeouts

The first UART implementation is intentionally polling/blocking:

```c
das_uart_write(uart, data, size);
das_uart_read(uart, data, size);
```

Finite-time operations are also available:

```c
das_uart_write_timeout(uart, data, size, 100u);
das_uart_read_timeout(uart, data, size, 100u);
```

Finite timeouts use the generic DAS monotonic time source and therefore require `das_time_init()` or an application/RTOS source installed with `das_time_set_source()`. `DAS_UART_WAIT_FOREVER` selects indefinite waiting without requiring the DAS timebase.

Receive parity, framing, noise, and overrun faults return `DAS_ERROR_IO`.

Interrupt-driven and DMA transfers are deliberately not hidden inside this first polling implementation. The generic IRQ abstraction already exists; asynchronous UART ownership, buffering, and callback semantics should be designed explicitly rather than smuggled into a blocking API.

## Focused hardware qualification

Before integrating UART into the full STM32H755 campaign, run the focused dual-core loopback test:

```bash
./scripts/stm32h755_uart_test.sh /home/dev/STM32Cube/Repository/STM32CubeH7/
```

Connect one jumper:

```text
Arduino D1 / TX / PB6  ->  Arduino D0 / RX / PB7
```

The test runs on CM7 and CM4 independently and checks:

- finite receive timeout with no incoming byte;
- board route resolution and GPIO AF setup;
- baud-rate derivation from live peripheral clocks;
- 115200 baud 8N1 loopback;
- 57600 baud 8E2 loopback;
- 38400 baud 7O1 loopback;
- deterministic byte-pattern integrity;
- continued execution after all transfers.

CM7 first selects the qualified 400 MHz board profile, so its USART1 input clock is derived from the resulting APB2 clock. The CM4 image runs after reset and independently exercises the CPU2 peripheral-clock enable path.

The first physical run of the baseline reached the first 7O1 byte after all 8N1 and 8E2 bytes passed, but received `0x80` for transmitted application data `0x00`. That exposed the STM32 parity position through the generic byte API. The backend fix masks the hardware register to the configured application data width; the focused test must pass again before UART is integrated into the full campaign.
