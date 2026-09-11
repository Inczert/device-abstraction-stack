# Interrupt model

DAS exposes interrupt-controller behavior through a device-agnostic API while keeping CMSIS and target-specific interrupt numbering below the public boundary.

The goal is not to reimplement NVIC. On Cortex-M, DAS deliberately uses CMSIS-Core internally; application code sees only `das_irq_t` and generic controller operations.

## Public contract

Header:

```c
#include <das/irq.h>
```

`das_irq_t` contains backend-owned storage. Applications obtain it from the resource that owns/resolves an interrupt source rather than constructing vendor IRQ numbers manually.

Current source APIs that resolve generic controller handles include:

- GPIO/EXTI via `das_gpio_interrupt_get_irq()`;
- periodic timer update via `das_timer_get_irq()`;
- DMA execution resources via `das_dma_get_irq()`;
- semantic board button interrupts via `das_board_button_interrupt_get_irq()`.

## Source versus controller

```text
peripheral/source configuration
        |
        v
GPIO / timer / DMA / board event
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
```

Source-specific event masks/pending/clear remain in the owning API. Controller enable/priority/pending remain in `das_irq_*()`. This distinction matters when several sources share one controller line.

## Controller API

```c
das_irq_enable()
das_irq_disable()
das_irq_is_enabled()

das_irq_set_priority()
das_irq_get_priority()
das_irq_priority_levels()

das_irq_set_pending()
das_irq_clear_pending()
das_irq_is_pending()
```

Priority values are logical levels:

```text
0                              highest priority
...
das_irq_priority_levels()-1   lowest priority
```

On Cortex-M, DAS maps these operations onto CMSIS NVIC helpers and the device-provided priority width.

## Shared lines

A `das_irq_t` identifies a controller line, not necessarily a unique peripheral source. STM32 GPIO EXTI lines are a direct example: several source lines share one NVIC vector. Enabling the controller therefore affects every source on that line, while each peripheral/source API remains responsible for identifying and clearing its own event.

## Handler and vector ownership

The current IRQ API controls interrupt-controller state. It does **not** provide generic runtime handler registration.

For STM32H755, DAS supplies one weak default vector table covering the complete fixed MCU vector layout. External vector slots point to their normal STM32/CMSIS handler symbols, and DAS supplies weak implementations of those symbols that fall through to `Default_Handler`.

Normal firmware therefore keeps the DAS vector table and overrides only the handlers it owns. For example:

```c
void TIM2_IRQHandler(void) {
    /* application, test or RTOS-owned TIM2 handling */
}
```

That strong definition replaces the weak DAS `TIM2_IRQHandler`; no copied vector table is required. The same model permits UART, SPI, I2C, DMA, GPIO interrupts and RTOS core handlers to coexist in one firmware image.

Core exception handlers supplied by the Cortex-M layer are weak as well. An RTOS can therefore provide strong `SysTick_Handler`, `PendSV_Handler` or fault handlers while retaining the DAS device vector table.

Whole-table replacement remains available for exceptional startup policies. A bootloader or application can provide a strong `g_das_vector_table`, or set `DAS_USE_DEFAULT_VECTOR_TABLE=OFF` and provide its own `.isr_vector` section. This is not required merely to bind a peripheral ISR.

Hardware qualification firmware uses the same default DAS table as normal applications and supplies only test-specific strong ISR functions. The dedicated custom-vector consumer test is intentionally the exception because its purpose is to validate whole-table replacement.

Portable callback/dispatch registration is a separate design problem involving shared lines, static/runtime binding and RTOS/application policy. DAS does not hide that problem inside `das_irq_enable()`.

## CMSIS boundary

```text
das_irq_*()
    -> CMSIS NVIC_* helpers
    -> NVIC hardware
```

The STM32H755 default vector-table implementation follows the CMSIS/ST device IRQ layout internally. No CMSIS or STM32 types appear in `<das/irq.h>`.

## Qualification

The standing STM32H755 campaign exercises IRQ controller semantics through physical GPIO/EXTI on CM7 and CM4, the board-button path, periodic TIM2 update delivery, and DMA IRQ resolution. It verifies controller state/priority/pending behavior while preserving source-specific pending/clear handling.

The previous completed **38/38** regression baseline predates the default-vector-table cleanup. The full hardware campaign must therefore be rerun after this change before the new vector ownership model inherits that qualification claim.
