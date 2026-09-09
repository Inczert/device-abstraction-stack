# Cortex-M core backend

This directory is the Cortex-M CPU/core implementation layer.

It intentionally contains no STM32 peripheral code. Planned work includes reusable startup/exception support, NVIC helpers, SysTick/timebase support, and Cortex-M7 cache/MPU helpers.

Device-specific interrupt routing, clocks, GPIO, serial peripherals and DMA belong under `src/device/<device>/`.
