# Public API reference

DAS public headers live under `include/das/`. Application code should use these headers rather than implementation files under `src/`. Public APIs use DAS and standard C types; CMSIS and STM32 types remain backend details.

## Public headers

```text
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
include/das/board.h
include/das/board_resources.h
include/das/cortex_m/startup.h
```

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

`DAS_ERROR_INVALID_ARGUMENT` covers invalid handles, pins, enum values, null pointers and invalid numeric ranges. `DAS_ERROR_UNSUPPORTED` means a valid generic operation is unavailable on the selected backend. `DAS_ERROR_TIMEOUT` reports a bounded operation that missed its deadline. `DAS_ERROR_NOT_READY` reports missing initialization/resource state. `DAS_ERROR_IO` reports a peripheral/DMA transport or data-path failure.

## Clock

Header: `<das/clock.h>`

Applications select a board frequency in hertz and query the resulting system/executing-core rate. The current NUCLEO-H755ZI-Q backend advertises 64, 200, 300 and 400 MHz. PLL dividers, voltage scale, FLASH latency and bus prescalers remain backend details.

CM7 owns global clock changes on STM32H755; CM4 may query the live tree but global frequency changes are unsupported from CPU2.

See [Clock control](clocks.md).

## Monotonic time

Header: `<das/time.h>`

The API provides a wrapping `uint32_t` millisecond timestamp, elapsed/deadline helpers, finite intervals, `das_delay_ms()`, default target initialization, and an application/RTOS source callback. The Cortex-M default uses a 1 kHz SysTick derived from the live executing-core frequency.

See [Monotonic time](time.md).

## Interrupt controller

Header: `<das/irq.h>`

`das_irq_t` is an opaque interrupt-controller handle. The API provides validity, enable/disable/query, priority-level query/set/get and pending set/clear/query. On Cortex-M the backend delegates to CMSIS NVIC internally.

GPIO, periodic timer and DMA APIs can resolve their controller line as a `das_irq_t`. Source-specific pending/enable/clear behavior remains in the owning peripheral API.

See [Interrupt model](interrupts.md).

## GPIO and EXTI

Header: `<das/gpio.h>`

The GPIO API covers:

- ports A through K as generic identifiers;
- input/output/alternate/analog modes;
- none/up/down pulls;
- push-pull/open-drain output type;
- low/medium/high/very-high speed;
- alternate-function selection;
- atomic logical write/toggle and input/output readback;
- rising/falling/both-edge interrupt configuration;
- source enable/pending/clear and IRQ resolution.

STM32 GPIO registers, EXTI/SYSCFG routing and NVIC numbers do not appear in the public contract.

## UART

Header: `<das/uart.h>`

`das_uart_t` is opaque. The current API supports:

- 7 or 8 **application** data bits;
- no/even/odd parity;
- one/two stop bits;
- blocking read/write;
- finite-time read/write using the DAS time source;
- effective baud-rate query;
- transmit flush;
- `DAS_UART_WAIT_FOREVER`.

The parity bit is never exposed as application data. Receive framing/parity/noise/overrun faults map to `DAS_ERROR_IO`.

Qualified board routes are ST-LINK VCP (USART3 internally) and Arduino D1/D0 (USART1 internally).

See [UART](uart.md).

## SPI

Header: `<das/spi.h>`

`das_spi_t` is opaque. The current byte-oriented controller API supports:

- modes 0, 1, 2 and 3;
- MSB-first and LSB-first;
- requested maximum/effective SCK frequency;
- blocking and finite-time full-duplex polling transfers;
- `tx == NULL` receive-only fill semantics and `rx == NULL` transmit-only discard semantics;
- full-duplex DMA transfer through implementation-selected DMA resources.

SPI transfer calls do not assert chip select. Board/default CS control is separate so applications can keep one assertion across several transfers.

See [SPI](spi.md).

## I2C

Header: `<das/i2c.h>`

`das_i2c_t` is opaque. The current controller baseline supports:

- 7-bit addressing;
- 100 kHz Standard mode and 400 kHz Fast mode;
- address-only probe;
- read/write;
- combined write/repeated-START/read;
- blocking and finite-time variants;
- nominal configured-frequency query;
- up to 255 bytes per non-empty phase in the current implementation.

NACK, arbitration loss, bus error and overrun map to `DAS_ERROR_IO`.

See [I2C](i2c.md).

## Timer and PWM

Header: `<das/timer.h>`

`das_timer_t` provides a generic periodic timer with requested/effective frequency, start/stop/running state, counter read/reset, update-event source enable/pending/clear and `das_irq_t` resolution.

`das_pwm_t` provides frequency and integer duty in per-mille (`0..1000`) with start/stop/running and duty/frequency readback. The current board semantic PWM route is Arduino D4.

See [Timers and PWM](timer.md).

## DMA

Header: `<das/dma.h>`

`das_dma_t` is an opaque acquired execution resource. The API provides:

- acquire/release/validity;
- peripheral-to-memory, memory-to-peripheral and memory-to-memory directions;
- byte/halfword/word element widths;
- independent source/destination increment semantics;
- configure/start/state/remaining-count;
- blocking and finite-time completion wait;
- abort;
- IRQ resolution.

The STM32H755 backend uses DMA1/DMAMUX1 internally. Stream and request identifiers are not public API.

See [DMA and cache coherency](dma.md).

## Data cache

Header: `<das/cache.h>`

The cache API exposes D-cache availability/enabled state/line size, enable/disable, and clean/invalidate/clean+invalidate range maintenance.

CM7 uses CMSIS cache primitives with 32-byte line-aware range expansion. CM4 has no D-cache; range maintenance is a successful no-op while enabling/disabling a nonexistent cache returns `DAS_ERROR_UNSUPPORTED`.

DMA does not perform cache maintenance automatically. Buffer ownership and coherency remain explicit at the call site.

## Board APIs

Headers: `<das/board.h>` and `<das/board_resources.h>`

Semantic board resources currently include:

```text
LED green/yellow/red     -> LD1/LD2/LD3
B1 USER                  -> semantic button API
ST-LINK VCP UART         -> PD8/PD9 route
Arduino UART             -> PB6/PB7 route
Arduino I2C              -> PB8/PB9 route
Arduino SPI              -> PA5/PA6/PB5 + PD14 CS
Arduino PWM D4           -> PE14
Arduino GPIO D3/D4       -> PE13/PE14 aliases
```

The board layer owns physical polarity, connector routing and alternate-function choices. Applications receive generic handles/pins rather than STM32 peripheral types.

See [Board resources](board.md).

## Cortex-M startup

Header: `<das/cortex_m/startup.h>`

DAS provides weak reusable `Reset_Handler` and core exception defaults. The reset path restores `.data`, clears `.bss`, writes VTOR and enters `main()`.

The **final firmware owns the vector table**. A bare-metal application retaining DAS startup must provide `.isr_vector` entries for the initial stack pointer, `Reset_Handler`, and every core/device handler it uses. This includes `SysTick_Handler` when using the default DAS time source.

## API design rule

DAS abstracts functionality when it provides a stable application contract or hides target-specific configuration. It does not duplicate CMSIS merely to rename it. Public headers remain vendor-type free; board/device/core implementation details stay in their owning layers.
