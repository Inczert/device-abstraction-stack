# NUCLEO-H755ZI-Q board resources

The DAS board layer exposes a deliberately small set of **semantic resources** for the NUCLEO-H755ZI-Q. It does not reproduce the entire STM32H755 pin table under different names.

A mapping belongs here when application code benefits from referring to a board function or connector route instead of a raw MCU pin/peripheral instance.

## Layer boundary

```text
application
    |
    v
semantic board resource
    |
    v
generic DAS peripheral/GPIO API
    |
    v
STM32H755 device backend
    |
    v
CMSIS definitions
```

## LEDs

| DAS resource | Board marking | MCU pin | Logical on |
| --- | --- | --- | --- |
| `DAS_BOARD_LED_GREEN` | LD1 | PB0 | high |
| `DAS_BOARD_LED_YELLOW` | LD2 | PE1 | high |
| `DAS_BOARD_LED_RED` | LD3 | PB14 | high |

The board API provides init/set/toggle/readback and pin resolution without requiring an application to know the physical pins.

## User button

```text
DAS_BOARD_BUTTON_USER -> B1 USER -> PC13
released -> low
pressed  -> high
```

The stock board provides the signal bias. DAS exposes logical press/release state plus press/release/both-event interrupt configuration, source enable/pending/clear and generic `das_irq_t` resolution.

## UART routes

| DAS resource | Purpose | TX | RX | Device fact |
| --- | --- | --- | --- | --- |
| `DAS_BOARD_UART_STLINK_VCP` | ST-LINK USB VCP | PD8 | PD9 | USART3 / AF7 |
| `DAS_BOARD_UART_ARDUINO` | Arduino D1/D0 | PB6 | PB7 | USART1 / AF7 |

`das_board_uart_init()` configures the route and returns an opaque `das_uart_t`. The STM32 USART instance and AF values remain board/backend facts.

## Arduino I2C

```text
DAS_BOARD_I2C_ARDUINO
SCL -> D15 / PB8 / I2C1_SCL AF4
SDA -> D14 / PB9 / I2C1_SDA AF4
```

`das_board_i2c_init()` configures alternate-function open-drain pins and returns an opaque `das_i2c_t`.

The qualification fixture uses I2C4 on PF14/PF15 as a test-only target; that endpoint is not exposed as a normal board resource.

## Arduino SPI

```text
DAS_BOARD_SPI_ARDUINO
SCK   -> PA5 / SPI1_SCK  AF5
MISO  -> PA6 / SPI1_MISO AF5
MOSI  -> PB5 / SPI1_MOSI AF5
CS    -> PD14 / GPIO, active low
```

`das_board_spi_init()` configures the bus and returns an opaque `das_spi_t`. Chip select remains transaction policy outside `das_spi_transfer*()`. `das_board_spi_chip_select()` controls the route's default active-low CS; applications can use arbitrary DAS GPIOs for additional devices.

## PWM route

```text
DAS_BOARD_PWM_ARDUINO_D4 -> D4 / PE14 -> TIM1_CH4 AF1 internally
```

`das_board_pwm_init()` returns an opaque `das_pwm_t`. Application code selects frequency/duty rather than timer instance/channel/AF fields.

## Qualification GPIO aliases

```text
DAS_BOARD_GPIO_ARDUINO_D3 -> PE13
DAS_BOARD_GPIO_ARDUINO_D4 -> PE14
```

The D4-to-D3 jumper is reused for physical GPIO loopback/open-drain/EXTI and PWM observation in the standing hardware campaign.

## Why not map every connector pin?

The NUCLEO exposes many MCU pins and alternate functions. Turning every pin into a second `DAS_BOARD_*` pinout table would add maintenance without adding abstraction.

Add a semantic resource when the board gives the signal a function, a normal routed peripheral connection matters to application code, or a stable qualification fixture benefits from a meaningful board name. One-off raw GPIO access remains available through `das_gpio_pin_t`.

## Hardware qualification

The current 38-case campaign physically qualifies:

- LED behavior;
- B1 polling and press/release EXTI;
- both UART routes' underlying backend through qualified paths;
- Arduino SPI polling and DMA loopback;
- Arduino I2C controller against a test-only I2C4 target;
- Arduino D4 PWM observed through D3;
- D3/D4 GPIO loopback/open-drain/EXTI.

The stock solder-bridge/configuration assumptions documented here are the qualified board profile. Modified board routing requires an explicit board configuration rather than guesswork.

## Source of truth

Board mappings are derived from ST's NUCLEO-H755ZI-Q user manual and MB1363 board schematic. Device/peripheral behavior remains implemented below this board layer.
