# Public API reference

DAS public headers live under `include/das/`. Application code should use these headers rather than implementation files under `src/`. Public APIs use DAS and standard C types; CMSIS and STM32 types remain backend details.

## Public headers

```text
include/das/das.h
include/das/result.h
include/das/clock.h
include/das/time.h
include/das/irq.h
include/das/gpio.h
include/das/cache.h
include/das/dma.h
include/das/uart.h
include/das/spi.h
include/das/i2c.h
include/das/timer.h
include/das/eth.h
include/das/board.h
include/das/board_resources.h
include/das/cortex_m/startup.h
```

`<das/das.h>` is the convenience umbrella for normal application-facing APIs. Cortex-M startup/vector customization remains deliberately explicit through `<das/cortex_m/startup.h>`.

## Result codes

```c
typedef enum das_result {
    DAS_OK = 0,
    DAS_ERROR_INVALID_ARGUMENT = -1,
    DAS_ERROR_UNSUPPORTED = -2,
    DAS_ERROR_TIMEOUT = -3,
    DAS_ERROR_NOT_READY = -4,
    DAS_ERROR_IO = -5
} das_result_t;
```

`DAS_ERROR_INVALID_ARGUMENT` covers invalid handles, enums, pointers and numeric ranges. `DAS_ERROR_UNSUPPORTED` means a valid generic operation is unavailable on the selected backend. `DAS_ERROR_TIMEOUT` reports a bounded operation that missed its deadline. `DAS_ERROR_NOT_READY` reports missing initialization/resource state. `DAS_ERROR_IO` reports a peripheral/DMA/data-path failure.

## Clock

Header: `<das/clock.h>`

Applications select a board frequency in hertz and query the resulting system/executing-core rate. The current NUCLEO-H755ZI-Q backend advertises 64, 200, 300 and 400 MHz. PLL dividers, voltage scale, FLASH latency and bus prescalers remain backend details.

CM7 owns global STM32H755 clock changes; CM4 may query the live tree but global frequency changes are unsupported from CPU2.

See [Clock control](clocks.md).

## Monotonic time

Header: `<das/time.h>`

The API provides a wrapping `uint32_t` millisecond timestamp, elapsed/deadline helpers, finite intervals, `das_delay_ms()`, default target initialization and an application/RTOS source callback. The Cortex-M default uses a 1 kHz SysTick derived from the live executing-core frequency.

See [Monotonic time](time.md).

## Interrupt controller

Header: `<das/irq.h>`

`das_irq_t` is an opaque interrupt-controller handle. The API provides validity, enable/disable/query, priority-level query/set/get and pending set/clear/query. On Cortex-M the backend delegates to CMSIS NVIC internally.

GPIO, periodic timer and generic DMA APIs can resolve their controller line as a `das_irq_t`. Source-specific event state remains in the owning peripheral API.

See [Interrupt model](interrupts.md).

## GPIO and EXTI

Header: `<das/gpio.h>`

The GPIO API covers ports A through K as generic identifiers; input/output/alternate/analog modes; pull configuration; push-pull/open-drain output; speed; alternate functions; logical write/toggle/readback; rising/falling/both-edge EXTI configuration; source enable/pending/clear; and IRQ resolution.

STM32 GPIO registers, EXTI/SYSCFG routing and NVIC numbers do not appear in the public contract.

## UART

Header: `<das/uart.h>`

`das_uart_t` is opaque. The current API supports 7 or 8 application data bits, no/even/odd parity, one/two stop bits, blocking and finite-time I/O, effective baud-rate query, transmit flush and `DAS_UART_WAIT_FOREVER`.

The parity bit is never exposed as application data. Receive framing/parity/noise/overrun faults map to `DAS_ERROR_IO`.

Qualified board routes are ST-LINK VCP and Arduino D1/D0.

See [UART](uart.md).

## SPI

Header: `<das/spi.h>`

`das_spi_t` is opaque. The byte-oriented controller API supports modes 0..3, both bit orders, requested/effective SCK frequency, blocking/finite-time polling transfers and full-duplex DMA transfer through implementation-selected generic DMA resources.

SPI transfer calls do not assert chip select. Board/default CS control remains separate so applications can keep one assertion across several transfers.

See [SPI](spi.md).

## I2C

Header: `<das/i2c.h>`

`das_i2c_t` is opaque. The current controller baseline supports 7-bit addressing, 100/400 kHz, probe, read, write, combined write/repeated-START/read, finite-time variants and configured-frequency query. Each non-empty phase is currently bounded to 255 bytes.

NACK, arbitration loss, bus error and overrun map to `DAS_ERROR_IO`.

See [I2C](i2c.md).

## Timer and PWM

Header: `<das/timer.h>`

`das_timer_t` provides requested/effective periodic frequency, start/stop/running state, counter read/reset, update-event source enable/pending/clear and `das_irq_t` resolution.

`das_pwm_t` provides frequency and integer duty in per-mille (`0..1000`) with start/stop/running and duty/frequency readback. The current board semantic PWM route is Arduino D4.

See [Timers and PWM](timer.md).

## Generic DMA

Header: `<das/dma.h>`

`das_dma_t` is an opaque acquired execution resource. The API provides acquire/release/validity; peripheral-to-memory, memory-to-peripheral and memory-to-memory directions; byte/halfword/word widths; independent increment policy; configure/start/state/remaining-count; finite or blocking wait; abort; and IRQ resolution.

The STM32H755 generic backend uses DMA1/DMAMUX1 internally. Stream and request identifiers are not public API.

The Ethernet peripheral has a separate descriptor-based DMA engine. Ethernet TX/RX is not routed through `das_dma_t`.

See [DMA and cache coherency](dma.md).

## Data cache

Header: `<das/cache.h>`

The cache API exposes D-cache availability/enabled state/line size, enable/disable and clean/invalidate/clean+invalidate range maintenance.

CM7 uses CMSIS cache primitives with 32-byte line-aware range expansion. CM4 has no D-cache; range maintenance is a successful no-op while enabling/disabling a nonexistent cache returns `DAS_ERROR_UNSUPPORTED`.

Generic DMA does not infer arbitrary caller-buffer ownership. Ethernet owns coherency for its private descriptor/buffer data path.

## Ethernet Layer 2

Header: `<das/eth.h>`

`das_eth_t` is opaque. Configuration currently contains the station MAC address:

```c
das_eth_config_t config = {
    .mac = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01},
};
```

The public Layer-2 operations are:

```c
das_eth_init(eth, &config);
das_eth_send(eth, frame, length);
das_eth_receive(eth, buffer, capacity, &received);
das_eth_link_state(eth, &state);
```

`das_eth_send()` operates on a complete Ethernet frame excluding FCS. `das_eth_receive()` is polling/non-blocking and returns `DAS_OK` with `received == 0` when idle. `das_eth_link_state()` returns link up/down, speed in Mbps and duplex state.

The qualified NUCLEO-H755ZI-Q semantic route is `DAS_BOARD_ETH_RJ45`. The current STM32H755 implementation is CM7-owned and polling-only; CM4 initialization returns `DAS_ERROR_UNSUPPORTED` before board routing is changed.

DAS stops at Layer 2. ARP, IP, DHCP, UDP, TCP, DNS, sockets and lwIP types are outside this API.

See [Ethernet](ethernet.md).

## Board APIs

Headers: `<das/board.h>` and `<das/board_resources.h>`

Semantic NUCLEO-H755ZI-Q resources currently include:

```text
LED green/yellow/red     -> LD1/LD2/LD3
B1 USER                  -> semantic button API
ST-LINK VCP UART         -> PD8/PD9 route
Arduino UART             -> PB6/PB7 route
Arduino I2C              -> PB8/PB9 route
Arduino SPI              -> PA5/PA6/PB5 + PD14 CS
Arduino PWM D4           -> PE14
Arduino GPIO D3/D4       -> PE13/PE14 aliases
RJ45 Ethernet            -> ETH1 RMII + LAN8742A
```

The board layer owns physical polarity, connector routing and alternate-function choices. Applications receive generic handles/pins rather than STM32 peripheral types.

See [Board resources](board.md).

## Cortex-M startup and vector ownership

Header: `<das/cortex_m/startup.h>`

DAS provides weak reusable `Reset_Handler` and core exception defaults. The reset path restores `.data`, clears `.bss`, programs VTOR and enters `main()`.

For STM32H755, DAS normally owns the canonical `.isr_vector` table. The table references weak standard core/device handler symbols. A normal application or RTOS binds an ISR by supplying a strong handler definition, for example `TIM2_IRQHandler`, `SysTick_Handler` or `PendSV_Handler`; it does **not** need to copy the vector table.

Whole-table replacement is supported only when firmware deliberately owns startup/vector policy: set `DAS_USE_DEFAULT_VECTOR_TABLE=OFF` and provide `.isr_vector`, or provide a strong `g_das_vector_table` definition.

See [Interrupt model](interrupts.md) and [Building and integration](integration.md).

## API design rule

DAS abstracts functionality when it provides a stable application contract or hides target-specific configuration. It does not duplicate CMSIS merely to rename it. Public headers remain vendor-type free; board/device/core implementation details stay in their owning layers.

## Qualification

The standing NUCLEO-H755ZI-Q hardware regression is **39/39 PASS** at DAS commit `f6b65672d9ae69cf28cd574d0dbba01cf875d8dc` (2026-09-13), including the CM7 polling Layer-2 Ethernet path.
