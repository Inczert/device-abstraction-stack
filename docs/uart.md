# UART

DAS exposes UART as a generic opaque handle plus configuration/transfer API. Applications do not use STM32 USART instances, RCC mux fields, BRR calculations, alternate-function selectors or CMSIS register types.

## Board-oriented setup

```c
const das_uart_config_t config = {
    .baud_rate = 115200u,
    .data_bits = DAS_UART_DATA_BITS_8,
    .parity = DAS_UART_PARITY_NONE,
    .stop_bits = DAS_UART_STOP_BITS_1,
};

das_uart_t uart = DAS_UART_INVALID;
(void)das_board_uart_init(DAS_BOARD_UART_STLINK_VCP, &config, &uart);
```

Current qualified routes:

| Resource | Physical route | Backend fact |
| --- | --- | --- |
| `DAS_BOARD_UART_STLINK_VCP` | PD8 TX / PD9 RX | USART3 / AF7 |
| `DAS_BOARD_UART_ARDUINO` | PB6 TX / PB7 RX | USART1 / AF7 |

## Framing

The byte API supports 7 or 8 **application data bits**, optional even/odd parity and one/two stop bits. `data_bits` excludes the parity bit. The STM32 backend masks hardware parity storage out of the application byte path.

Qualified framing includes:

```text
115200 8N1
57600  8E2
38400  7O1
```

Nine application data bits are intentionally not exposed through the current byte-oriented API.

## I/O and timeouts

Blocking:

```c
das_uart_write(uart, data, size);
das_uart_read(uart, data, size);
```

Finite-time:

```c
das_uart_write_timeout(uart, data, size, 100u);
das_uart_read_timeout(uart, data, size, 100u);
```

Finite timeouts use the DAS monotonic source. `DAS_UART_WAIT_FOREVER` selects indefinite waiting without requiring a time source. `das_uart_get_baud_rate()` reports the effective rate and `das_uart_flush()` waits for complete transmit shift-out.

Receive parity, framing, noise and overrun faults map to `DAS_ERROR_IO`.

The current UART baseline is polling/blocking. A future asynchronous/interrupt-driven buffering/callback model should be designed explicitly rather than smuggled into these calls.

## Clocking

The STM32H755 backend derives the relevant APB peripheral clock from live DAS clock state and calculates BRR at runtime. Applications do not assume CPU clock equals UART clock.

## Hardware qualification

The focused qualifier remains available for fast iteration:

```bash
./scripts/stm32h755_uart_test.sh /home/dev/STM32Cube/Repository/STM32CubeH7/
```

Fixture:

```text
Arduino D1 / TX / PB6 <-> Arduino D0 / RX / PB7
```

Both CM7 and CM4 verify timeout behavior, semantic route setup, live baud generation, all three advertised framing combinations, exact deterministic byte equality and continued execution.

UART is already promoted into the standing campaign and is included in the completed **38/38** STM32H755 regression baseline.
