# NUCLEO-H755ZI-Q board resources

The DAS board layer exposes a deliberately small set of **semantic resources** for the NUCLEO-H755ZI-Q. It is not intended to reproduce the STM32H755 pin table under different names.

The guiding rule is simple: a mapping belongs here when applications, examples, or hardware-qualification code benefit from referring to a board function rather than a raw MCU pin.

## Layer boundary

```text
application / test
        |
        v
semantic board resource
        |
        v
generic DAS GPIO / peripheral API
        |
        v
STM32H755 backend
        |
        v
CMSIS device definitions
```

Board code owns facts such as "B1 is active high" or "the ST-LINK VCP is wired to these two MCU pins". The generic GPIO/UART/I2C/SPI implementations do not.

## User button

The stock board's blue B1 USER button is exposed as:

```c
DAS_BOARD_BUTTON_USER
```

Default board routing:

```text
B1 USER -> PC13
logical released -> low
logical pressed  -> high
```

The board has a hardware pull-down on the B1 signal, so DAS configures PC13 as an input without adding an internal pull. Press/release polarity is hidden by the board API.

Typical polling use:

```c
(void)das_board_button_init(DAS_BOARD_BUTTON_USER);

if (das_board_button_is_pressed(DAS_BOARD_BUTTON_USER)) {
    /* button is physically pressed */
}
```

For interrupts:

```c
das_irq_t irq = DAS_IRQ_INVALID;

(void)das_board_button_interrupt_configure(
    DAS_BOARD_BUTTON_USER,
    DAS_BOARD_BUTTON_EVENT_BOTH);
(void)das_board_button_interrupt_get_irq(DAS_BOARD_BUTTON_USER, &irq);
(void)das_irq_enable(irq);
(void)das_board_button_interrupt_enable(DAS_BOARD_BUTTON_USER, true);
```

The board layer maps `PRESS` and `RELEASE` onto the correct GPIO edge. Applications do not need to know that the current board is active high or that the signal is PC13.

The current default mapping assumes the stock NUCLEO solder-bridge routing for B1. A board modified to route the button differently is outside this qualified profile until its board configuration is represented explicitly.

## LEDs

Existing LED resources remain unchanged:

| DAS resource | Board marking | MCU pin | Logical on |
| --- | --- | --- | --- |
| `DAS_BOARD_LED_GREEN` | LD1 | PB0 | high |
| `DAS_BOARD_LED_YELLOW` | LD2 | PE1 | high |
| `DAS_BOARD_LED_RED` | LD3 | PB14 | high |

## UART connections

`include/das/board_resources.h` defines named UART **connections** rather than STM32 peripheral instances.

| DAS resource | Purpose | TX | RX |
| --- | --- | --- | --- |
| `DAS_BOARD_UART_STLINK_VCP` | ST-LINK USB virtual COM path | PD8 | PD9 |
| `DAS_BOARD_UART_ARDUINO` | Arduino/Zio D1/D0 serial pair | PB6 | PB7 |

The current resource API returns pins only. The future UART backend (#8) owns USART/LPUART configuration, alternate-function selection, baud generation, interrupts and data transfer. This keeps board wiring separate from peripheral implementation.

The ST-LINK VCP mapping assumes the stock solder-bridge configuration connecting PD8/PD9 to the ST-LINK virtual COM interface.

## Arduino/Zio I2C

| DAS resource | Connector signals | SCL | SDA |
| --- | --- | --- | --- |
| `DAS_BOARD_I2C_ARDUINO` | D15 / D14 | PB8 | PB9 |

The future I2C backend (#12) owns peripheral configuration and electrical protocol behavior. This board resource only identifies the routed connector pins.

## Arduino/Zio SPI

| DAS resource | SCK | MISO | MOSI | board CS GPIO |
| --- | --- | --- | --- | --- |
| `DAS_BOARD_SPI_ARDUINO` | PA5 | PA6 | PB5 | PD14 |

The `cs` member is intentionally a normal GPIO resource associated with the connector. DAS does not assume every SPI device must use hardware NSS or that every attached device shares one chip-select policy.

The future SPI backend (#11) owns the SPI peripheral, alternate functions, clocking and transfers.

## Qualification GPIO pair

The established electrical-loopback fixture is now represented semantically:

```text
DAS_BOARD_GPIO_ARDUINO_D4 -> PE14 -> CN10 pin 8
DAS_BOARD_GPIO_ARDUINO_D3 -> PE13 -> CN10 pin 10
```

The physical campaign connects D4 to D3 with one jumper for output/input, open-drain and EXTI qualification. Keeping these names in the board layer lets the fixture remain understandable without scattering `PE13` and `PE14` through future test code.

## Why not map every connector pin?

The NUCLEO exposes many MCU pins and alternate functions. Turning every one into a `DAS_BOARD_*` alias would create a second pinout table that has to be maintained forever while providing almost no abstraction value.

New semantic resources should be added when at least one of these is true:

- the board gives the signal a physical function, such as B1 or an LED;
- a routed connection matters to normal use, such as ST-LINK VCP;
- a connector group is the natural way a generic peripheral is consumed, such as Arduino I2C/SPI/UART;
- a stable hardware-qualification fixture needs a named board endpoint.

Raw one-off GPIO access remains available through `das_gpio_pin_t`.

## Hardware qualification

Issue #15 adds a dedicated CM7 board-resource image to the existing campaign. The image validates the published resource table in firmware and then physically qualifies B1:

1. B1 must initially read released;
2. the user presses and holds B1;
3. DAS must observe the logical pressed state and a press EXTI event;
4. the user releases B1;
5. DAS must observe the logical released state and a release EXTI event.

The test uses the generic `das_irq_t` controller path together with the semantic board-button source API. Mechanical switch bounce is intentionally tolerated: qualification requires at least one press and one release event rather than pretending a push-button is a precision pulse generator.

The established CM7/CM4 GPIO, IRQ, clock and timebase cases remain part of the same archive.

## Source of truth

Board mappings are derived from ST's NUCLEO-H755ZI-Q user manual and MB1363 board schematic. When a mapping depends on a solder-bridge option, DAS documents and qualifies the stock/default board configuration rather than guessing the state of a modified board.
