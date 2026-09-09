# Public API reference

DAS public headers live under:

```text
include/das/
```

Application code should use these headers rather than implementation files under `src/`. Public APIs use DAS and standard C types; CMSIS and STM32 types remain backend details.

Current public headers:

```text
include/das/result.h
include/das/clock.h
include/das/irq.h
include/das/gpio.h
include/das/board.h
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
    DAS_ERROR_TIMEOUT = -3
} das_result_t;
```

`DAS_ERROR_INVALID_ARGUMENT` covers invalid handles, pins, enum values, null output pointers and unsupported numeric ranges. `DAS_ERROR_UNSUPPORTED` is used when a valid generic operation or requested profile is unavailable on the selected backend/board. `DAS_ERROR_TIMEOUT` reports a bounded hardware transition that did not reach its required state.

## Clock API

```c
#include <das/clock.h>
```

Applications select a frequency in hertz. PLL dividers, voltage scaling, FLASH latency, bus prescalers and physical board power configuration remain backend details.

```c
das_result_t das_clock_set_frequency(uint32_t frequency_hz);
das_result_t das_clock_get_frequency(uint32_t* frequency_hz);
bool das_clock_frequency_supported(uint32_t frequency_hz);
size_t das_clock_get_supported_frequencies(uint32_t* frequencies_hz,
                                           size_t capacity);
```

A typical application can therefore do:

```c
if (das_clock_frequency_supported(400000000u)) {
    (void)das_clock_set_frequency(400000000u);
}
```

The NUCLEO-H755ZI-Q backend currently advertises 64, 200, 300 and 400 MHz. On the stock board 480 MHz is deliberately not advertised because the default direct-SMPS power path is limited to the VOS1 operating range. See [Clock control](clocks.md).

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

and obtain a real handle from the DAS resource associated with the interrupt source. For GPIO:

```c
(void)das_gpio_interrupt_get_irq(pin, &irq);
```

`das_irq_is_valid()` checks whether a handle is usable by the selected controller backend.

### Enable state

```c
das_result_t das_irq_enable(das_irq_t irq);
das_result_t das_irq_disable(das_irq_t irq);
das_result_t das_irq_is_enabled(das_irq_t irq, bool* enabled);
```

These functions control the CPU interrupt-controller line. They do not configure or clear the peripheral source itself.

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

The application does not need to know the controller register field width. On Cortex-M, DAS uses CMSIS `__NVIC_PRIO_BITS` and the CMSIS NVIC helpers internally.

### Controller pending state

```c
das_result_t das_irq_set_pending(das_irq_t irq);
das_result_t das_irq_clear_pending(das_irq_t irq);
das_result_t das_irq_is_pending(das_irq_t irq, bool* pending);
```

This is interrupt-controller pending state. It is separate from a peripheral's own pending/event status.

For the full interrupt model, shared IRQ-line semantics, and the CMSIS boundary, see [Interrupt model](interrupts.md).

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

For STM32H7, alternate-function selectors 0 through 15 are supported. The backend writes the requested output latch before entering output/alternate mode to avoid an unintended transient.

Convenience initialization:

```c
das_result_t das_gpio_input_init(
    das_gpio_pin_t pin,
    das_gpio_pull_t pull);
das_result_t das_gpio_output_init(
    das_gpio_pin_t pin,
    bool initial_high);
das_result_t das_gpio_output_init_ex(
    das_gpio_pin_t pin,
    das_gpio_output_type_t output_type,
    das_gpio_pull_t pull,
    das_gpio_speed_t speed,
    bool initial_high);
```

### GPIO I/O

```c
das_result_t das_gpio_write(das_gpio_pin_t pin, bool high);
das_result_t das_gpio_toggle(das_gpio_pin_t pin);
bool das_gpio_read_output(das_gpio_pin_t pin);
bool das_gpio_read_input(das_gpio_pin_t pin);
```

`das_gpio_read_output()` reads the output latch; `das_gpio_read_input()` reads the physical pad state. These can differ, particularly for open-drain outputs.

The STM32H755 backend uses `BSRR` for atomic set/reset.

## GPIO interrupt-source API

Edge selection:

```text
DAS_GPIO_INTERRUPT_RISING
DAS_GPIO_INTERRUPT_FALLING
DAS_GPIO_INTERRUPT_BOTH
```

Source routing/configuration:

```c
das_result_t das_gpio_interrupt_configure(
    das_gpio_pin_t pin,
    das_gpio_interrupt_edge_t edge);
```

On STM32H755 this configures SYSCFG/EXTI source routing and trigger selection for the selected core. It does not directly enable the CPU interrupt-controller line.

Source mask:

```c
das_result_t das_gpio_interrupt_enable(
    das_gpio_pin_t pin,
    bool enabled);
```

Resolve the controller line:

```c
das_result_t das_gpio_interrupt_get_irq(
    das_gpio_pin_t pin,
    das_irq_t* irq);
```

Several source lines can share one controller handle. On STM32H755, for example, GPIO EXTI lines 10 through 15 share a controller IRQ. Enabling/disabling that `das_irq_t` therefore affects the shared controller line, while source-specific state remains in the GPIO/EXTI API.

Source pending state:

```c
bool das_gpio_interrupt_pending(das_gpio_pin_t pin);
das_result_t das_gpio_interrupt_clear(das_gpio_pin_t pin);
```

### Typical GPIO interrupt setup

```c
static const das_gpio_pin_t irq_pin = {
    DAS_GPIO_PORT_E,
    13u
};

void init_irq(void)
{
    das_irq_t irq = DAS_IRQ_INVALID;

    if (das_gpio_input_init(irq_pin, DAS_GPIO_PULL_DOWN) != DAS_OK ||
        das_gpio_interrupt_configure(
            irq_pin,
            DAS_GPIO_INTERRUPT_BOTH) != DAS_OK ||
        das_gpio_interrupt_get_irq(irq_pin, &irq) != DAS_OK) {
        return;
    }

    const uint32_t levels = das_irq_priority_levels();
    const uint32_t priority = levels > 1u ? levels / 2u : 0u;

    (void)das_irq_set_priority(irq, priority);
    (void)das_irq_clear_pending(irq);
    (void)das_irq_enable(irq);
    (void)das_gpio_interrupt_enable(irq_pin, true);
}
```

The application no longer needs `IRQn_Type`, `EXTI15_10_IRQn`, or `NVIC_*` for controller setup.

The current API does not yet provide portable handler registration. The final image still owns its vector table/ISR entry points, and the handler uses source-specific pending/clear functions to service shared lines.

## Board API

```c
#include <das/board.h>
```

Current NUCLEO-H755ZI-Q LED resources:

```text
DAS_BOARD_LED_GREEN   -> LD1 / PB0  / active high
DAS_BOARD_LED_YELLOW  -> LD2 / PE1  / active high
DAS_BOARD_LED_RED     -> LD3 / PB14 / active high
```

Functions:

```c
das_result_t das_board_led_init(das_board_led_t led, bool initially_on);
das_result_t das_board_led_init_all(bool initially_on);
das_result_t das_board_led_set(das_board_led_t led, bool on);
das_result_t das_board_led_toggle(das_board_led_t led);
bool das_board_led_is_on(das_board_led_t led);
das_gpio_pin_t das_board_led_pin(das_board_led_t led);
```

Use semantic board resources when application intent is board-level. `das_board_led_pin()` is available when code deliberately needs the underlying generic GPIO.

## Cortex-M startup API

```c
#include <das/cortex_m/startup.h>
```

DAS provides weak reset/runtime and core-exception symbols for bare-metal Cortex-M integration. Applications, bootloaders and RTOSes may replace the weak definitions.

The default reset path restores `.data`, clears `.bss`, writes VTOR from the linker contract and calls `main()`.

## API design rule

DAS abstracts functionality where a stable user-facing contract provides portability or hides target-specific configuration. It does not duplicate CMSIS merely to rename it.

The interrupt API demonstrates the distinction:

```text
public:   das_irq_enable(das_irq_t)
backend:  CMSIS NVIC_EnableIRQ(...)
```

The public contract is portable; the backend reuses the standard low-level implementation.

DAS remains early (`0.1.x`). Public APIs are intended to stabilize over time, but compatibility should not yet be assumed across development revisions.
