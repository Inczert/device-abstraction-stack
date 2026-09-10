# I2C controller

DAS exposes I2C through a generic controller API. Application code selects a semantic board route and bus rate; STM32 instance numbers, RCC selectors, TIMINGR fields and GPIO alternate functions stay below the public API.

## Public API

The current baseline supports 7-bit controller addressing at Standard-mode 100 kHz and Fast-mode 400 kHz.

```c
#include <das/board_resources.h>
#include <das/i2c.h>

const das_i2c_config_t config = {
    .frequency_hz = 400000u,
};

das_i2c_t i2c = DAS_I2C_INVALID;
if (das_board_i2c_init(DAS_BOARD_I2C_ARDUINO, &config, &i2c) != DAS_OK) {
    /* handle error */
}

uint8_t reg = 0x10u;
uint8_t value[4] = {0};
(void)das_i2c_write_read_timeout(i2c,
                                 0x52u,
                                 &reg,
                                 1u,
                                 value,
                                 sizeof(value),
                                 50u);
```

Available operations are:

- address-only probe;
- blocking write;
- blocking read;
- combined write-then-read using a repeated START;
- finite-time variants of all controller transactions;
- nominal configured bus-frequency query.

The first baseline limits each non-empty transaction phase to 255 bytes. This matches one STM32H755 `NBYTES` phase without exposing the STM32 `RELOAD` mechanism in the portable contract. Larger transactions can be added later without changing normal application code.

`DAS_I2C_WAIT_FOREVER` selects the blocking path. Finite timeouts use the generic DAS monotonic time source and therefore require `das_time_init()` or an injected application/RTOS time source.

## Error behavior

The STM32H755 backend maps controller NACK, arbitration loss, bus error and overrun into `DAS_ERROR_IO`. A finite transaction that does not complete before its deadline returns `DAS_ERROR_TIMEOUT` rather than spinning indefinitely.

The API deliberately does not expose STM32 ISR/ICR flags. More detailed portable I2C error reporting can be added later if applications have a concrete need for it.

## NUCLEO-H755ZI-Q route

The semantic Arduino I2C resource is:

```text
DAS_BOARD_I2C_ARDUINO
SCL  Arduino D15  PB8  I2C1_SCL  AF4
SDA  Arduino D14  PB9  I2C1_SDA  AF4
```

`das_board_i2c_init()` configures both lines as alternate-function open-drain signals and returns only an opaque `das_i2c_t`.

The board baseline enables internal pull-ups. They make the short same-board qualification fixture self-contained, but real external I2C wiring should use pull-up resistors selected for bus voltage, capacitance and required rise time rather than treating MCU pull-ups as a universal bus design.

## Clock/timing policy

I2C1/2/3 use the STM32H755 I2C123 kernel-clock selector. The current backend explicitly selects the live HSI source and derives its rate from the RCC HSI divider state.

TIMINGR is calculated at runtime for 100 kHz or 400 kHz from that live kernel frequency. No CubeMX-generated timing literal or assumed CPU/APB rate is part of the application contract.

## Focused physical qualification

I2C cannot be meaningfully qualified by tying one signal to itself. The focused test therefore uses two real I2C controllers on the same MCU:

```text
DAS controller under test                    test-only target

Arduino D15 / PB8 / I2C1_SCL  ---------->  Zio D69 / PF14 / I2C4_SCL
Arduino D14 / PB9 / I2C1_SDA  ---------->  Zio D68 / PF15 / I2C4_SDA
                                             CN9 pins 19 / 21
```

PF14/PF15 use I2C4 AF4 on STM32H755. I2C4 exists only as qualification infrastructure in this test and is programmed directly through CMSIS in the test firmware; it is not exposed as a second public board resource merely to serve the test bench.

Run:

```bash
./scripts/stm32h755_i2c_test.sh /home/dev/STM32Cube/Repository/STM32CubeH7/
```

Each core independently tests I2C1 as controller against the interrupt-serviced I2C4 target at address `0x52`. The qualifier checks:

- semantic board mapping and open-drain AF setup;
- 100 kHz nominal timing;
- successful address probe;
- expected NACK from unused address `0x53`;
- physical controller write and target receive;
- physical controller read and target transmit;
- combined write/repeated-START/read;
- reconfiguration to 400 kHz and another combined transaction;
- exact byte equality across 70 checked application bytes;
- no I2C4 target bus/arbitration/overrun errors;
- continued execution after all transactions.

As with UART, SPI and timer/PWM, this focused qualifier must pass on both CM7 and CM4 before the two I2C cases are promoted into the main hardware campaign.
