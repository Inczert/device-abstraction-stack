# MCU/core layer

`src/mcu/` contains CPU/core architecture support only.

For Cortex-M, valid code includes exception/core helpers, NVIC/SCB access, SysTick, interrupt masking, cache/MPU helpers, and reusable startup primitives. Implementations may be C or assembly.

Vendor device peripherals do not belong here. STM32 GPIO, RCC, USART, DMA, EXTI/SYSCFG, timers, SPI, I2C, ADC and similar code belongs under `src/device/<device>/`.

The goal is that a Cortex-M core implementation can be reused by multiple silicon devices without acquiring STM32-specific dependencies.
