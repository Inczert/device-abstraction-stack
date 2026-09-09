# Public API reference

The public API lives under:

```text
include/das/
```

Applications should include these headers rather than backend files under `src/`.

The API is C11 and currently consists of common result codes, generic GPIO access, and board-level LED helpers for the NUCLEO-H755ZI-Q.

## Result codes

Header:

```c
#include <das/result.h>
```

Type:

```c
typedef enum das_result {
    DAS_OK = 0,
    DAS_ERROR_INVALID_ARGUMENT = -1,
    DAS_ERROR_UNSUPPORTED = -2
} das_result_t;
```

### `DAS_OK`

The operation completed successfully.

### `DAS_ERROR_INVALID_ARGUMENT`

One or more arguments are outside the supported range or otherwise invalid. Examples include an invalid GPIO port, pin number greater than 15, invalid alternate-function number, or invalid configuration enum value.

### `DAS_ERROR_UNSUPPORTED`

Reserved for operations that are valid at the generic API level but not supported by a selected backend/device.

## GPIO API

Header:

```c
#include <das/gpio.h>
```

### GPIO pins

A GPIO is identified by port and pin number:

```c
typedef struct das_gpio_pin {
    das_gpio_port_t port;
    uint8_t pin;
} das_gpio_pin_t;
```

Example:

```c
const das_gpio_pin_t pe14 = {
    .port = DAS_GPIO_PORT_E,
    .pin = 14u,
};
```

Supported generic port identifiers currently span `DAS_GPIO_PORT_A` through `DAS_GPIO_PORT_K`. A backend may reject a port that does not exist on its target.

### Modes

```c
DAS_GPIO_MODE_INPUT
DAS_GPIO_MODE_OUTPUT
DAS_GPIO_MODE_ALTERNATE
DAS_GPIO_MODE_ANALOG
```

### Pull configuration

```c
DAS_GPIO_PULL_NONE
DAS_GPIO_PULL_UP
DAS_GPIO_PULL_DOWN
```

### Output type

```c
DAS_GPIO_OUTPUT_PUSH_PULL
DAS_GPIO_OUTPUT_OPEN_DRAIN
```

For open-drain outputs, writing logical high releases the line. The physical high level therefore requires a pull-up, either external or intentionally configured internally where electrically appropriate.

### Output speed

```c
DAS_GPIO_SPEED_LOW
DAS_GPIO_SPEED_MEDIUM
DAS_GPIO_SPEED_HIGH
DAS_GPIO_SPEED_VERY_HIGH
```

These map to the backend's available GPIO slew/speed settings. They are electrical drive configuration, not a request for a software toggle frequency.

### Full GPIO configuration

```c
das_result_t das_gpio_configure(
    das_gpio_pin_t pin,
    const das_gpio_config_t* config);
```

Configuration structure:

```c
typedef struct das_gpio_config {
    das_gpio_mode_t mode;
    das_gpio_pull_t pull;
    das_gpio_output_type_t output_type;
    das_gpio_speed_t speed;
    uint8_t alternate;
    bool initial_high;
} das_gpio_config_t;
```

Example alternate-function configuration:

```c
const das_gpio_pin_t pin = { DAS_GPIO_PORT_B, 6u };
const das_gpio_config_t config = {
    .mode = DAS_GPIO_MODE_ALTERNATE,
    .pull = DAS_GPIO_PULL_NONE,
    .output_type = DAS_GPIO_OUTPUT_PUSH_PULL,
    .speed = DAS_GPIO_SPEED_HIGH,
    .alternate = 7u,
    .initial_high = true,
};

if (das_gpio_configure(pin, &config) != DAS_OK) {
    /* handle configuration error */
}
```

For STM32H7, alternate-function values are accepted in the range 0 to 15.

When configuring an output or alternate-function output, the STM32H7 backend writes the requested initial output latch before switching the GPIO mode. This avoids a needless transient caused by entering output mode with an unintended previous latch state.

### Input convenience function

```c
das_result_t das_gpio_input_init(
    das_gpio_pin_t pin,
    das_gpio_pull_t pull);
```

Example:

```c
const das_gpio_pin_t input = { DAS_GPIO_PORT_E, 13u };
(void)das_gpio_input_init(input, DAS_GPIO_PULL_UP);

if (das_gpio_read_input(input)) {
    /* input is high */
}
```

### Output convenience functions

Simple push-pull output:

```c
das_result_t das_gpio_output_init(
    das_gpio_pin_t pin,
    bool initial_high);
```

Extended output configuration:

```c
das_result_t das_gpio_output_init_ex(
    das_gpio_pin_t pin,
    das_gpio_output_type_t output_type,
    das_gpio_pull_t pull,
    das_gpio_speed_t speed,
    bool initial_high);
```

Example open-drain output using an internal pull-up:

```c
const das_gpio_pin_t line = { DAS_GPIO_PORT_E, 14u };

(void)das_gpio_output_init_ex(
    line,
    DAS_GPIO_OUTPUT_OPEN_DRAIN,
    DAS_GPIO_PULL_UP,
    DAS_GPIO_SPEED_LOW,
    true);
```

Whether an internal pull-up is electrically suitable depends on the actual bus/load. DAS configures the requested MCU feature; it does not repeal Ohm's law on behalf of the application.

### Writing and toggling

```c
das_result_t das_gpio_write(das_gpio_pin_t pin, bool high);
das_result_t das_gpio_toggle(das_gpio_pin_t pin);
```

Example:

```c
(void)das_gpio_write(line, false);
(void)das_gpio_write(line, true);
(void)das_gpio_toggle(line);
```

The STM32H7 backend uses `BSRR` for atomic set/reset operations.

### Reading output and input state

```c
bool das_gpio_read_output(das_gpio_pin_t pin);
bool das_gpio_read_input(das_gpio_pin_t pin);
```

Semantics:

- `das_gpio_read_output()` reads the configured output latch (`ODR` on STM32H7);
- `das_gpio_read_input()` reads the physical input state (`IDR` on STM32H7).

These are intentionally different. For example, an open-drain output may have an output latch of high/released while the actual pin is held low by another device.

Invalid pins return `false` from the boolean read functions. Use configuration/result-returning APIs when invalid-argument distinction matters.

## GPIO interrupt / EXTI API

### Edge selection

```c
DAS_GPIO_INTERRUPT_RISING
DAS_GPIO_INTERRUPT_FALLING
DAS_GPIO_INTERRUPT_BOTH
```

### Configure an interrupt source

```c
das_result_t das_gpio_interrupt_configure(
    das_gpio_pin_t pin,
    das_gpio_interrupt_edge_t edge);
```

On STM32H7 this:

1. enables the SYSCFG clock;
2. routes the selected GPIO port into the correct EXTI source field;
3. configures rising/falling trigger bits;
4. masks the EXTI line while configuration is completed;
5. clears stale pending state.

It does **not** configure the Cortex-M NVIC vector or priority.

### Enable or disable the EXTI line

```c
das_result_t das_gpio_interrupt_enable(
    das_gpio_pin_t pin,
    bool enabled);
```

This controls the EXTI interrupt mask for the line. The application still owns NVIC enable/priority and the actual ISR function.

### Pending state

```c
bool das_gpio_interrupt_pending(das_gpio_pin_t pin);
```

Returns whether the selected GPIO line has pending EXTI state.

### Clear pending state

```c
das_result_t das_gpio_interrupt_clear(das_gpio_pin_t pin);
```

Call this from the application's IRQ handler after handling the event as required by the selected backend.

### Typical interrupt ownership

Conceptually:

```c
static const das_gpio_pin_t irq_pin = {
    DAS_GPIO_PORT_E,
    13u
};

void init_irq(void)
{
    (void)das_gpio_input_init(irq_pin, DAS_GPIO_PULL_DOWN);
    (void)das_gpio_interrupt_configure(
        irq_pin,
        DAS_GPIO_INTERRUPT_BOTH);

    /* Application-owned Cortex-M setup. */
    NVIC_SetPriority(EXTI15_10_IRQn, 5u);
    NVIC_EnableIRQ(EXTI15_10_IRQn);

    (void)das_gpio_interrupt_enable(irq_pin, true);
}

void EXTI15_10_IRQHandler(void)
{
    if (das_gpio_interrupt_pending(irq_pin)) {
        const bool level = das_gpio_read_input(irq_pin);
        (void)level;
        (void)das_gpio_interrupt_clear(irq_pin);
    }
}
```

The `NVIC_*` calls and vector name are deliberately not wrapped by the GPIO API. They belong to the application/architecture policy.

## Board API

Header:

```c
#include <das/board.h>
```

The current board layer provides semantic access to NUCLEO-H755ZI-Q user LEDs.

### LEDs

```c
DAS_BOARD_LED_GREEN
DAS_BOARD_LED_YELLOW
DAS_BOARD_LED_RED
```

Current physical mapping:

| DAS resource | Board LED | GPIO | Active level |
| --- | --- | --- | --- |
| `DAS_BOARD_LED_GREEN` | LD1 | PB0 | high |
| `DAS_BOARD_LED_YELLOW` | LD2 | PE1 | high |
| `DAS_BOARD_LED_RED` | LD3 | PB14 | high |

### Initialize one LED

```c
das_result_t das_board_led_init(
    das_board_led_t led,
    bool initially_on);
```

### Initialize all LEDs

```c
das_result_t das_board_led_init_all(bool initially_on);
```

### Set or toggle

```c
das_result_t das_board_led_set(das_board_led_t led, bool on);
das_result_t das_board_led_toggle(das_board_led_t led);
```

### Read logical board state

```c
bool das_board_led_is_on(das_board_led_t led);
```

### Resolve the backing GPIO

```c
das_gpio_pin_t das_board_led_pin(das_board_led_t led);
```

This is useful when an application needs to bridge from a semantic board resource to generic GPIO functionality. Most application code should prefer the semantic LED API when all it needs is LED behavior.

## Error-handling style

Configuration and state-changing calls return `das_result_t`.

A strict application can propagate failures:

```c
das_result_t init_status_led(void)
{
    return das_board_led_init(DAS_BOARD_LED_GREEN, false);
}
```

Simple bring-up code may intentionally ignore a result when the arguments are compile-time constants and failure is impossible for the selected board:

```c
(void)das_board_led_set(DAS_BOARD_LED_GREEN, true);
```

The explicit cast makes that choice visible rather than accidentally discarding a result.

## API stability

DAS is still early (`0.1.x` project stage). The public API is intended to become stable, but compatibility should not be assumed until the project begins issuing stable release contracts.

New peripheral APIs should follow the same principles:

- generic public types;
- no vendor types in public headers;
- small convenience wrappers over a complete configuration primitive where appropriate;
- backend-specific details isolated below the public boundary;
- real-target qualification before a backend is considered mature.
