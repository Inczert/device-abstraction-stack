# Public API reference

DAS public headers live under `include/das/`. Application code should use these headers rather than implementation files under `src/`. Public APIs use DAS and standard C types; CMSIS and STM32 types remain backend details.

Current public headers:

```text
include/das/result.h
include/das/clock.h
include/das/time.h
include/das/irq.h
include/das/gpio.h
include/das/board.h
include/das/board_resources.h
include/das/cortex_m/startup.h
```

## Result codes

```c
#include <das/result.h>
```

```c
typedef enum das_result {
    DAS_OK = 0,
    DAS_ERROR_INVALID_ARGUMENT = -1,
    DAS_ERROR_UNSUPPORTED = -2,
    DAS_ERROR_TIMEOUT = -3,
    DAS_ERROR_NOT_READY = -4
} das_result_t;
```

`DAS_ERROR_INVALID_ARGUMENT` covers invalid handles, pins, enum values, null output pointers and unsupported numeric ranges. `DAS_ERROR_UNSUPPORTED` means a valid generic operation/profile is unavailable on the selected backend/board. `DAS_ERROR_TIMEOUT` reports a bounded hardware transition that did not reach its required state. `DAS_ERROR_NOT_READY` reports an operation that requires prior initialization or an installed resource.

## Clock API

```c
#include <das/clock.h>
```

Applications select a board frequency in hertz. PLL dividers, voltage scaling, FLASH latency, bus prescalers and board power configuration remain backend details.

```c
das_result_t das_clock_set_frequency(uint32_t frequency_hz);
das_result_t das_clock_get_frequency(uint32_t* frequency_hz);
das_result_t das_clock_get_core_frequency(uint32_t* frequency_hz);
bool das_clock_frequency_supported(uint32_t frequency_hz);
size_t das_clock_get_supported_frequencies(uint32_t* frequencies_hz,
                                           size_t capacity);
```

The current NUCLEO-H755ZI-Q backend advertises 64, 200, 300 and 400 MHz. `das_clock_get_frequency()` returns the primary/system frequency; `das_clock_get_core_frequency()` returns the frequency of the core executing the selected DAS build, which can differ on a multi-core device.

See [Clock control](clocks.md).

## Monotonic time API

```c
#include <das/time.h>
```

The public timestamp type is a wrapping millisecond counter:

```c
typedef uint32_t das_time_ms_t;
#define DAS_TIME_MAX_INTERVAL_MS UINT32_C(0x7fffffff)
```

Initialize the target default source:

```c
das_result_t das_time_init(void);
```

Read/use it:

```c
bool das_time_is_ready(void);
das_time_ms_t das_time_now_ms(void);
uint32_t das_time_elapsed_ms(das_time_ms_t start_ms);
bool das_time_interval_elapsed(das_time_ms_t start_ms,
                               uint32_t interval_ms);
das_result_t das_time_deadline_after(uint32_t delay_ms,
                                     das_time_ms_t* deadline_ms);
bool das_time_deadline_reached(das_time_ms_t deadline_ms);
das_result_t das_delay_ms(uint32_t duration_ms);
```

An RTOS/application can replace the default source:

```c
typedef das_time_ms_t (*das_time_source_fn_t)(void* context);
das_result_t das_time_set_source(das_time_source_fn_t source,
                                 void* context);
```

On the current Cortex-M target the default backend uses CMSIS `SysTick_Config()` at 1 kHz, calculated from the live executing-core frequency. Public callers do not interact with SysTick or `SystemCoreClock`.

See [Monotonic time](time.md).

## Interrupt-controller API

```c
#include <das/irq.h>
```

### Interrupt handles

```c
typedef struct das_irq {
    uint32_t storage;
} das_irq_t;
```

`storage` is backend-owned. Applications must not interpret it as an NVIC number or construct handles from vendor constants. Initialize an empty handle with:

```c
das_irq_t irq = DAS_IRQ_INVALID;
```

and obtain a real handle from the DAS resource associated with the interrupt source, for example GPIO or a board button.

### Enable state

```c
das_result_t das_irq_enable(das_irq_t irq);
das_result_t das_irq_disable(das_irq_t irq);
das_result_t das_irq_is_enabled(das_irq_t irq, bool* enabled);
```

These functions control the CPU interrupt-controller line. They do not configure or clear the peripheral/source itself.

### Priority

```c
uint32_t das_irq_priority_levels(void);
das_result_t das_irq_set_priority(das_irq_t irq, uint32_t priority);
das_result_t das_irq_get_priority(das_irq_t irq, uint32_t* priority);
```

Priority semantics are:

```text
0                              highest priority
...
das_irq_priority_levels()-1   lowest priority
```

On Cortex-M, DAS maps this onto CMSIS `NVIC_*` and `__NVIC_PRIO_BITS` internally.

### Controller pending state

```c
das_result_t das_irq_set_pending(das_irq_t irq);
das_result_t das_irq_clear_pending(das_irq_t irq);
das_result_t das_irq_is_pending(das_irq_t irq, bool* pending);
```

Controller pending state is separate from a peripheral/source event flag.

See [Interrupt model](interrupts.md).

## GPIO API

```c
#include <das/gpio.h>
```

### Pins

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

Generic port identifiers currently span `DAS_GPIO_PORT_A` through `DAS_GPIO_PORT_K`. A backend may reject ports absent on its target.

### Configuration

Modes:

```text
DAS_GPIO_MODE_INPUT
DAS_GPIO_MODE_OUTPUT
DAS_GPIO_MODE_ALTERNATE
DAS_GPIO_MODE_ANALOG
```

Pulls:

```text
DAS_GPIO_PULL_NONE
DAS_GPIO_PULL_UP
DAS_GPIO_PULL_DOWN
```

Output types:

```text
DAS_GPIO_OUTPUT_PUSH_PULL
DAS_GPIO_OUTPUT_OPEN_DRAIN
```

Speeds:

```text
DAS_GPIO_SPEED_LOW
DAS_GPIO_SPEED_MEDIUM
DAS_GPIO_SPEED_HIGH
DAS_GPIO_SPEED_VERY_HIGH
```

Complete configuration:

```c
typedef struct das_gpio_config {
    das_gpio_mode_t mode;
    das_gpio_pull_t pull;
    das_gpio_output_type_t output_type;
    das_gpio_speed_t speed;
    uint8_t alternate;
    bool initial_high;
} das_gpio_config_t;

das_result_t das_gpio_configure(
    das_gpio_pin_t pin,
    const das_gpio_config_t* config);
```

Convenience initialization:

```c
das_result_t das_gpio_input_init(das_gpio_pin_t pin,
                                 das_gpio_pull_t pull);
das_result_t das_gpio_output_init(das_gpio_pin_t pin,
                                  bool initial_high);
das_result_t das_gpio_output_init_ex(
    das_gpio_pin_t pin,
    das_gpio_output_type_t output_type,
    das_gpio_pull_t pull,
    das_gpio_speed_t speed,
    bool initial_high);
```

I/O:

```c
das_result_t das_gpio_write(das_gpio_pin_t pin, bool high);
das_result_t das_gpio_toggle(das_gpio_pin_t pin);
bool das_gpio_read_output(das_gpio_pin_t pin);
bool das_gpio_read_input(das_gpio_pin_t pin);
```

For STM32H7 the backend uses `BSRR` for atomic set/reset and supports alternate-function selectors 0 through 15.

### GPIO interrupt source

```c
typedef enum das_gpio_interrupt_edge {
    DAS_GPIO_INTERRUPT_RISING,
    DAS_GPIO_INTERRUPT_FALLING,
    DAS_GPIO_INTERRUPT_BOTH
} das_gpio_interrupt_edge_t;

das_result_t das_gpio_interrupt_configure(
    das_gpio_pin_t pin,
    das_gpio_interrupt_edge_t edge);
das_result_t das_gpio_interrupt_enable(das_gpio_pin_t pin,
                                       bool enabled);
das_result_t das_gpio_interrupt_get_irq(das_gpio_pin_t pin,
                                        das_irq_t* irq);
bool das_gpio_interrupt_pending(das_gpio_pin_t pin);
das_result_t das_gpio_interrupt_clear(das_gpio_pin_t pin);
```

Several source lines can share one controller handle. On STM32H755, GPIO EXTI lines 10 through 15 share a controller IRQ, so source-specific masking/pending remains in the GPIO API while controller enable/priority remains in `das_irq_*()`.

## Board API

```c
#include <das/board.h>
```

### User LEDs

Current NUCLEO-H755ZI-Q resources:

```text
DAS_BOARD_LED_GREEN   -> LD1 / PB0  / active high
DAS_BOARD_LED_YELLOW  -> LD2 / PE1  / active high
DAS_BOARD_LED_RED     -> LD3 / PB14 / active high
```

Functions:

```c
das_result_t das_board_led_init(das_board_led_t led,
                                bool initially_on);
das_result_t das_board_led_init_all(bool initially_on);
das_result_t das_board_led_set(das_board_led_t led, bool on);
das_result_t das_board_led_toggle(das_board_led_t led);
bool das_board_led_is_on(das_board_led_t led);
das_gpio_pin_t das_board_led_pin(das_board_led_t led);
```

### User button

The stock blue B1 USER button is represented semantically:

```c
DAS_BOARD_BUTTON_USER
```

Applications do not need to know the underlying pin or active polarity:

```c
das_result_t das_board_button_init(das_board_button_t button);
bool das_board_button_is_pressed(das_board_button_t button);
das_gpio_pin_t das_board_button_pin(das_board_button_t button);
```

Interrupt source API:

```c
typedef enum das_board_button_event {
    DAS_BOARD_BUTTON_EVENT_PRESS,
    DAS_BOARD_BUTTON_EVENT_RELEASE,
    DAS_BOARD_BUTTON_EVENT_BOTH
} das_board_button_event_t;

das_result_t das_board_button_interrupt_configure(
    das_board_button_t button,
    das_board_button_event_t event);
das_result_t das_board_button_interrupt_enable(
    das_board_button_t button,
    bool enabled);
bool das_board_button_interrupt_pending(das_board_button_t button);
das_result_t das_board_button_interrupt_clear(das_board_button_t button);
das_result_t das_board_button_interrupt_get_irq(
    das_board_button_t button,
    das_irq_t* irq);
```

On the NUCLEO-H755ZI-Q default board configuration B1 is PC13 with a board pull-down and active-high press. Those electrical details remain in the board backend.

## Semantic connector resources

```c
#include <das/board_resources.h>
```

These mappings identify useful board connections without exposing STM32 peripheral types.

### UART connections

```c
typedef enum das_board_uart_resource {
    DAS_BOARD_UART_STLINK_VCP,
    DAS_BOARD_UART_ARDUINO,
    DAS_BOARD_UART_COUNT
} das_board_uart_resource_t;

typedef struct das_board_uart_pins {
    das_gpio_pin_t tx;
    das_gpio_pin_t rx;
} das_board_uart_pins_t;

das_result_t das_board_uart_get_pins(
    das_board_uart_resource_t resource,
    das_board_uart_pins_t* pins);
```

Current board mapping:

```text
ST-LINK VCP  -> PD8 / PD9
Arduino UART -> PB6 / PB7
```

### I2C connection

```c
typedef struct das_board_i2c_pins {
    das_gpio_pin_t scl;
    das_gpio_pin_t sda;
} das_board_i2c_pins_t;

das_result_t das_board_i2c_get_pins(
    das_board_i2c_resource_t resource,
    das_board_i2c_pins_t* pins);
```

`DAS_BOARD_I2C_ARDUINO` resolves to PB8/PB9.

### SPI connection

```c
typedef struct das_board_spi_pins {
    das_gpio_pin_t sck;
    das_gpio_pin_t miso;
    das_gpio_pin_t mosi;
    das_gpio_pin_t cs;
} das_board_spi_pins_t;

das_result_t das_board_spi_get_pins(
    das_board_spi_resource_t resource,
    das_board_spi_pins_t* pins);
```

`DAS_BOARD_SPI_ARDUINO` resolves to PA5/PA6/PB5 with PD14 as the connector-associated chip-select GPIO.

### Qualification GPIOs

```c
DAS_BOARD_GPIO_ARDUINO_D3
das_board_gpio_pin(DAS_BOARD_GPIO_ARDUINO_D3);

DAS_BOARD_GPIO_ARDUINO_D4
das_board_gpio_pin(DAS_BOARD_GPIO_ARDUINO_D4);
```

These resolve to PE13 and PE14 respectively and name the established physical loopback fixture used by the hardware campaign.

The resource API deliberately stops short of creating aliases for every NUCLEO connector pin. See [NUCLEO board resources](board.md).

## Cortex-M startup API

```c
#include <das/cortex_m/startup.h>
```

DAS provides weak reset/runtime and core-exception symbols for bare-metal Cortex-M integration. Applications, bootloaders and RTOSes may replace the weak definitions.

The default reset path restores `.data`, clears `.bss`, writes VTOR from the linker contract and calls `main()`.

## API design rule

DAS abstracts functionality where a stable user-facing contract provides portability or hides target-specific configuration. It does not duplicate CMSIS merely to rename it.

For example:

```text
public:   das_irq_enable(das_irq_t)
backend:  CMSIS NVIC_EnableIRQ(...)
```

Similarly, board APIs describe physical board intent while lower layers keep silicon details:

```text
public:   DAS_BOARD_BUTTON_USER
board:    PC13, active-high, board pull-down
device:   GPIO + EXTI
core:     NVIC through generic DAS IRQ API
```

DAS remains early (`0.1.x`). Public APIs are intended to stabilize over time, but compatibility should not yet be assumed across development revisions.
