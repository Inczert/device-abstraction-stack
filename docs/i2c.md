# I2C controller

DAS exposes I2C through a generic opaque controller handle. Applications select a semantic board route and bus rate; STM32 instance numbers, RCC selectors, TIMINGR fields and GPIO alternate functions stay below the public boundary.

## Public API

The current baseline supports 7-bit controller addressing at 100 kHz Standard mode and 400 kHz Fast mode.

```c
const das_i2c_config_t config = {
    .frequency_hz = 400000u,
};

das_i2c_t i2c = DAS_I2C_INVALID;
(void)das_board_i2c_init(DAS_BOARD_I2C_ARDUINO, &config, &i2c);
```

Available operations:

- address-only probe;
- blocking and finite-time write;
- blocking and finite-time read;
- combined write/repeated-START/read;
- configured-frequency query.

The current implementation bounds each non-empty transaction phase to 255 bytes. Longer transfers can later add backend reload/chunking without exposing STM32 `RELOAD` semantics in the portable contract.

Finite timeouts use the DAS monotonic source. NACK, arbitration loss, bus error and overrun map to `DAS_ERROR_IO`.

## NUCLEO-H755ZI-Q route

```text
DAS_BOARD_I2C_ARDUINO
SCL  Arduino D15  PB8  I2C1_SCL  AF4
SDA  Arduino D14  PB9  I2C1_SDA  AF4
```

`das_board_i2c_init()` configures alternate-function open-drain signaling and returns only `das_i2c_t`.

The current board baseline enables internal pull-ups for the short same-board qualification fixture. Real external buses should use physical pull-up resistors selected for bus voltage/capacitance/rise-time requirements.

## Clock/timing policy

The STM32H755 backend explicitly selects the I2C123 kernel clock source and calculates TIMINGR at runtime from the live frequency. No CubeMX-generated timing literal is part of the application contract.

## Hardware qualification

I2C cannot be qualified meaningfully by shorting one signal to itself, so the physical test uses a second real controller as a test-only endpoint:

```text
DAS I2C1 controller                        test-only I2C4 target
D15 / PB8 / I2C1_SCL  ---------------->  D69 / PF14 / I2C4_SCL
D14 / PB9 / I2C1_SDA  ---------------->  D68 / PF15 / I2C4_SDA
                                           address 0x52
```

Run the focused qualifier with:

```bash
./scripts/stm32h755_i2c_test.sh /home/dev/STM32Cube/Repository/STM32CubeH7/
```

Each core verifies 100/400 kHz timing, successful probe at `0x52`, expected NACK at `0x53`, physical write/read, repeated-START write/read, exact equality across 70 checked application bytes, no target-side bus/arbitration/overrun errors and continued execution.

I2C is already promoted into the standing campaign and is included in the completed **38/38** STM32H755 regression baseline.
