# Interrupt model

DAS exposes interrupt-controller behavior through a device-agnostic API while keeping CMSIS and target-specific interrupt numbering below the public boundary.

The goal is **not** to reimplement NVIC. On Cortex-M, DAS deliberately uses CMSIS-Core internally. The value of the abstraction is that application code does not depend on `IRQn_Type`, `NVIC_*`, STM32 interrupt numbers, or the interrupt controller used by a future non-Cortex-M backend.

## Public contract

Header:

```c
#include <das/irq.h>
```

An interrupt-controller line is represented by:

```c
das_irq_t
```

The handle contains backend-owned storage. Applications must obtain a handle from the DAS resource that owns or resolves the interrupt source. They must not construct controller numbers manually.

For GPIO interrupts:

```c
das_irq_t irq = DAS_IRQ_INVALID;

if (das_gpio_interrupt_get_irq(pin, &irq) != DAS_OK) {
    /* handle error */
}
```

Future UART, timer, DMA and other APIs can expose their controller line in the same way when direct application control is useful.

## Source versus controller

Interrupt handling has separate layers:

```text
peripheral/source configuration
        |
        v
GPIO EXTI / UART RX / timer / DMA event
        |
        v
interrupt-controller line -> das_irq_t
        |
        v
das_irq_enable / priority / pending
        |
        v
backend controller
        |
        +-- Cortex-M -> CMSIS NVIC
        +-- future architecture -> its native controller
```

For GPIO on STM32H755:

- `das_gpio_interrupt_configure()` selects the GPIO/EXTI source and edge;
- `das_gpio_interrupt_enable()` gates that EXTI source for the selected CPU;
- `das_gpio_interrupt_get_irq()` resolves the controller line;
- `das_irq_*()` controls the CPU interrupt controller;
- `das_gpio_interrupt_pending()` / `das_gpio_interrupt_clear()` operate on the peripheral/source pending state.

Controller pending state and peripheral pending state are intentionally not the same API.

## Controller API

```c
das_result_t das_irq_enable(das_irq_t irq);
das_result_t das_irq_disable(das_irq_t irq);
das_result_t das_irq_is_enabled(das_irq_t irq, bool* enabled);

das_result_t das_irq_set_priority(das_irq_t irq, uint32_t priority);
das_result_t das_irq_get_priority(das_irq_t irq, uint32_t* priority);
uint32_t das_irq_priority_levels(void);

das_result_t das_irq_set_pending(das_irq_t irq);
das_result_t das_irq_clear_pending(das_irq_t irq);
das_result_t das_irq_is_pending(das_irq_t irq, bool* pending);
```

`das_irq_is_valid()` can be used to validate a handle against the selected backend.

## Priority semantics

Priority values are controller priority **levels**, not raw register fields.

```text
0                              highest priority
...
das_irq_priority_levels()-1   lowest priority
```

The supported number of levels is queried at runtime/build-target level:

```c
const uint32_t levels = das_irq_priority_levels();
```

On Cortex-M the backend uses the device-provided CMSIS `__NVIC_PRIO_BITS` and `NVIC_SetPriority()` / `NVIC_GetPriority()` implementation. DAS does not copy their bit shifting or register layout.

Applications that need relative priority can therefore select values against `levels` rather than knowing how many NVIC priority bits a particular MCU implements.

## Shared interrupt lines

A `das_irq_t` identifies an interrupt-controller line, not necessarily one unique peripheral event.

STM32 GPIO is an immediate example: EXTI lines 5 through 9 share one NVIC line and EXTI lines 10 through 15 share another. Therefore different GPIO sources can resolve to the same `das_irq_t`.

Consequences:

- enabling/disabling the controller handle affects every source sharing that controller line;
- the handler must inspect peripheral/source pending state to determine what actually fired;
- source-specific clearing remains in the peripheral API.

This distinction will also matter for other devices with grouped/shared interrupt vectors.

## Typical GPIO use

```c
static const das_gpio_pin_t input = {
    DAS_GPIO_PORT_E,
    13u
};

void configure_input_irq(void)
{
    das_irq_t irq = DAS_IRQ_INVALID;

    (void)das_gpio_input_init(input, DAS_GPIO_PULL_DOWN);
    (void)das_gpio_interrupt_configure(
        input,
        DAS_GPIO_INTERRUPT_BOTH);

    if (das_gpio_interrupt_get_irq(input, &irq) != DAS_OK) {
        return;
    }

    const uint32_t levels = das_irq_priority_levels();
    const uint32_t priority = levels > 1u ? levels / 2u : 0u;

    (void)das_irq_set_priority(irq, priority);
    (void)das_irq_clear_pending(irq);
    (void)das_irq_enable(irq);
    (void)das_gpio_interrupt_enable(input, true);
}
```

The application does not need to know that PE13 maps to STM32 `EXTI15_10_IRQn` or that Cortex-M uses NVIC.

## Handler binding

The current IRQ API controls the interrupt controller. It does **not** yet provide portable runtime handler registration.

The final firmware image still owns its vector table and concrete ISR entry points. Current STM32 hardware qualification therefore uses the device vector name in the test image while all controller operations go through `das_irq_*()`.

Portable callback/handler registration is a separate design problem because it involves vector-table ownership, shared interrupt lines, static versus runtime binding, and RTOS/application policy. It should be added only with an explicit dispatch model rather than hidden inside controller enable calls.

## CMSIS boundary

For the Cortex-M backend:

```text
das_irq_*()
    -> CMSIS NVIC_* helpers
    -> NVIC hardware
```

The selected CMSIS device header supplies `IRQn_Type` and the implemented priority width required by CMSIS-Core. This is an internal build dependency. No CMSIS or STM32 types appear in `include/das/irq.h`.

This is the intended DAS boundary: abstract user-facing semantics, reuse CMSIS implementation machinery, and avoid copying vendor/architecture definitions merely to give them different names.

## Qualification

The STM32H755 hardware campaign exercises the IRQ API through the physical GPIO/EXTI test on both cores. The EXTI case verifies:

- GPIO source-to-controller handle resolution;
- controller disable/query;
- software pending set/query/clear;
- priority set/get round-trip;
- controller enable/query;
- real rising and falling GPIO edge delivery;
- source pending inspection and clearing in the ISR.

The same test runs against CM7 and CM4, so the public IRQ contract is exercised through both STM32H755 CPU interrupt-controller paths.
