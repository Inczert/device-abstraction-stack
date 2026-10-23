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

For the **v0.2.0 development branch**, bounded **nonblocking, partial-transfer** primitives are also available:

```c
size_t received = 0;
das_result_t result = das_uart_try_read(uart, rx, sizeof(rx), &received);
```

`das_uart_try_read` returns immediately with the number of bytes already waiting in the UART hardware (zero if none). `das_uart_try_write` immediately fills available transmit hardware space and reports bytes accepted; it does not wait for the last bit to shift out. An error in the receiver returns `DAS_ERROR_IO` with any earlier successful bytes counted. Both operations use the existing UART config and **do not depend on a monotonic time source**.

**Important limitation:** these are still hardware-FIFO polling operations, **not asynchronous reception**. Polling on a sleeping HardRT task can still lose data between calls. Issue #38 also tracks **IRQ-backed caller-owned bounded RX buffering**, added below. Sustained physical burst/load qualification and OBC integration acceptance are still pending. No HardRT or COBS handling belongs inside DAS.

## Optional interrupt-backed buffered RX (v0.2.0 development)

After UART initialization, a firmware can retain bytes in caller-owned storage
even while its application task is not scheduled:

```c
uint8_t rx_storage[256];
das_uart_rx_buffer_start(uart, rx_storage, sizeof(rx_storage));

size_t count = 0u;
das_uart_rx_buffer_read(uart, rx_chunk, sizeof(rx_chunk), &count);

das_uart_rx_stats_t stats;
das_uart_rx_buffer_stats(uart, &stats);

/* Before freeing rx_storage, resetting or reinitializing the UART: */
das_uart_rx_buffer_stop(uart);
```

The ring is bounded, single ISR producer / single task consumer, and retains
all `capacity` slots. When full, **new** bytes are discarded and counted;
oldest unread bytes are never overwritten. `bytes_stored`,
`bytes_dropped`, `hardware_errors` and `bytes_available` make data loss
visible. Counters wrap after 2^32 events and reset on start. UART errors
clear the appropriate peripheral flags and discard malformed RX bytes. The
IRQ service drains at most 32 characters per interrupt entry to bound
execution time; a pending source is subsequently serviced again.

Do **not** run polling/timeout/direct RX APIs concurrently with this mode;
they access the same peripheral RX data register. When buffering is active,
the backend rejects these direct reads and UART reinitialization with
`DAS_ERROR_NOT_READY`. Stop buffering before reinitializing the UART or
releasing its storage. The UART IRQ handler lives
in a separate archive object: **polling-only clients do not link the buffered
RX service**. The canonical vector table provides weak USART default handlers
that dispatch into the buffered service only if it was linked. An application
can override another USART's IRQ without duplicate handlers; for the UART
using DAS buffered RX, the application must not override the default handler
without forwarding events to the DAS service.
DAS installs no RTOS callback and does not manipulate the task scheduler.
Hardware priorities remain configurable through the normal interrupt
controller but must be set by the owning application as appropriate.

**Qualification boundary:** existing loopback tests are expanded to exercise
interrupt receive on both CM7 and CM4. Physical burst/overrun/latency
campaign with a connected Pi/host remains a separate acceptance gate on #38,
and is not implied by build success or a short loopback.

## Clocking

The STM32H755 backend derives the relevant APB peripheral clock from live DAS clock state and calculates BRR at runtime. Applications do not assume CPU clock equals UART clock.

## Hardware qualification

Focused qualifier:

```bash
./scripts/stm32h755_uart_test.sh /home/dev/STM32Cube/Repository/STM32CubeH7/
```

Fixture:

```text
Arduino D1 / TX / PB6 <-> Arduino D0 / RX / PB7
```

Both CM7 and CM4 qualify the original timeout/framing/byte-equality behavior plus development-branch immediate I/O and interrupt-ring loopback, including full-buffer overflow accounting. A physical Pi/host sustained-load test remains pending.

UART is part of the standing **40/40 PASS** STM32H755 regression baseline at DAS commit `737863075dfed78f73f99ae58f6e0ca52ffea93e`.
