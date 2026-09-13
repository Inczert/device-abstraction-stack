# Interrupt model

DAS exposes interrupt-controller behavior through a device-agnostic API while keeping CMSIS and target-specific interrupt numbering below the public boundary.

The goal is not to reimplement NVIC. On Cortex-M, DAS uses CMSIS-Core internally; application code sees `das_irq_t` and generic controller operations.

## Public contract

Header:

```c
#include <das/irq.h>
```

`das_irq_t` contains backend-owned storage. Applications obtain it from the resource that owns/resolves an interrupt source rather than constructing vendor IRQ numbers manually.

Current source APIs that resolve generic controller handles include:

- GPIO/EXTI via `das_gpio_interrupt_get_irq()`;
- periodic timer update via `das_timer_get_irq()`;
- generic DMA execution resources via `das_dma_get_irq()`;
- semantic board button interrupts via `das_board_button_interrupt_get_irq()`.

The current Ethernet baseline is polling-only and therefore does not expose ETH IRQ ownership yet.

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
CMSIS NVIC backend
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

## Handler and vector ownership

The IRQ API controls interrupt-controller state. It does **not** provide generic runtime handler registration.

For STM32H755, DAS owns one canonical weak vector table covering the complete fixed MCU layout. External vector slots reference their standard STM32/CMSIS handler symbols, and DAS supplies weak definitions that fall through to `Default_Handler`.

Normal firmware therefore keeps the DAS vector table and overrides only the handlers it owns:

```c
void TIM2_IRQHandler(void)
{
    /* application, test or RTOS-owned handling */
}
```

That strong definition replaces the weak DAS `TIM2_IRQHandler`; no copied vector table is required. The same model applies to UART, SPI, I2C, DMA, GPIO/EXTI and other peripheral handlers.

Core exception handlers supplied by the Cortex-M layer are weak as well. An RTOS can provide strong `SysTick_Handler`, `PendSV_Handler` or fault handlers while retaining the DAS device vector table. The HardRT integration uses exactly this model.

Whole-table replacement remains available only for firmware that genuinely owns startup/vector policy. Such firmware may provide a strong `g_das_vector_table`, or set `DAS_USE_DEFAULT_VECTOR_TABLE=OFF` and provide its own `.isr_vector`.

Hardware qualification firmware uses the same default DAS table as normal applications and supplies only test-specific strong ISR functions. The dedicated custom-vector consumer is intentionally the exception because its purpose is to validate complete table replacement.

Portable runtime callback/dispatch registration is a separate design problem involving shared lines, static/runtime binding and RTOS/application policy. DAS does not hide that problem inside `das_irq_enable()`.

## CMSIS boundary

```text
das_irq_*()
    -> CMSIS NVIC_* helpers
    -> NVIC hardware
```

The STM32H755 vector implementation follows the CMSIS/ST device IRQ layout internally. No CMSIS or STM32 types appear in `<das/irq.h>`.

## Qualification

The current STM32H755 campaign is **39/39 PASS** at DAS commit `f6b65672d9ae69cf28cd574d0dbba01cf875d8dc` (2026-09-13).

It exercises the canonical vector model on real CM7 and CM4 hardware through GPIO/EXTI, B1 button interrupts and TIM2 delivery, plus generic DMA IRQ resolution. CI additionally enforces one DAS vector table per regular hardware-test ELF, validates whole-table replacement separately, and verifies the HardRT strong `HardFault_Handler`, `PendSV_Handler` and `SysTick_Handler` coexist with the DAS-owned table.
