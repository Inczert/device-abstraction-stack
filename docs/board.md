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
generic DAS peripheral API
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

The stock board provides the signal bias. DAS exposes logical state plus press/release/both-event interrupt configuration, source enable/pending/clear and generic `das_irq_t` resolution.

## UART routes

| DAS resource | Purpose | TX | RX | Device fact |
| --- | --- | --- | --- | --- |
| `DAS_BOARD_UART_STLINK_VCP` | ST-LINK USB VCP | PD8 | PD9 | USART3 / AF7 |
| `DAS_BOARD_UART_ARDUINO` | Arduino D1/D0 | PB6 | PB7 | USART1 / AF7 |

`das_board_uart_init()` configures the route and returns an opaque `das_uart_t`.

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

`das_board_spi_init()` returns an opaque `das_spi_t`. Chip select remains transaction policy outside `das_spi_transfer*()`. `das_board_spi_chip_select()` controls the route's default active-low CS.

## PWM route

```text
DAS_BOARD_PWM_ARDUINO_D4 -> D4 / PE14 -> TIM1_CH4 AF1 internally
```

`das_board_pwm_init()` returns an opaque `das_pwm_t`.

## Qualification GPIO aliases

```text
DAS_BOARD_GPIO_ARDUINO_D3 -> PE13
DAS_BOARD_GPIO_ARDUINO_D4 -> PE14
```

The D4-to-D3 jumper is reused for physical GPIO loopback/open-drain/EXTI and PWM observation.

## RJ45 Ethernet

```text
DAS_BOARD_ETH_RJ45
    -> STM32H755 ETH1 MAC / dedicated ETH DMA
    -> RMII
    -> on-board LAN8742A PHY
    -> CN14 RJ45
```

The board layer owns the physical RMII route:

```text
PA1   RMII_REF_CLK
PA2   RMII_MDIO
PC1   RMII_MDC
PA7   RMII_CRS_DV
PC4   RMII_RXD0
PC5   RMII_RXD1
PG11  RMII_TX_EN
PG13  RMII_TXD0
PB13  RMII_TXD1
```

The standard qualified Ethernet setup has JP6 and JP7 fitted. `das_board_eth_init()` configures the route and returns an opaque `das_eth_t`; applications do not receive STM32 ETH/LAN8742 register types.

The current runtime ownership is CM7-only. CM4 returns `DAS_ERROR_UNSUPPORTED` before changing the board route.

## Why not map every connector pin?

The NUCLEO exposes many MCU pins and alternate functions. Turning every pin into a second `DAS_BOARD_*` pinout table would add maintenance without adding abstraction.

Add a semantic resource when the board gives the signal a function, a normal routed peripheral connection matters to application code, or a stable qualification fixture benefits from a meaningful board name. One-off raw GPIO access remains available through `das_gpio_pin_t`.

## Hardware qualification

The current **39/39 PASS** campaign physically qualifies:

- LED behavior;
- B1 polling and press/release EXTI;
- UART backend routes through the standing UART cases;
- Arduino SPI polling and DMA loopback;
- Arduino I2C controller against a test-only I2C4 target;
- Arduino D4 PWM observed through D3;
- D3/D4 GPIO loopback/open-drain/EXTI;
- CN14 Ethernet physical carrier and CM7 bidirectional raw Layer-2 traffic through LAN8742A/RMII/MAC/DMA with D-cache enabled.

The accepted campaign is DAS commit `f6b65672d9ae69cf28cd574d0dbba01cf875d8dc`, run on 2026-09-13.

The stock solder-bridge/jumper assumptions documented here are the qualified board profile. Modified board routing requires an explicit board configuration rather than guesswork.

## Source of truth

Board mappings are derived from ST's NUCLEO-H755ZI-Q user manual and MB1363 board schematic. Device/peripheral behavior remains implemented below this board layer.
