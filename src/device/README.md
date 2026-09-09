# Device layer

`src/device/` contains silicon/device-specific implementation.

For STM32H755 this is where on-chip peripherals and device control belong: GPIO, RCC/PWR/FLASH, EXTI/SYSCFG, USART/LPUART, DMA/DMAMUX, timers, SPI, I2C, ADC, watchdog and dual-core device control.

Device code may use the selected CMSIS device header internally. It must not encode NUCLEO connector wiring or other board-specific assumptions, and vendor types must not leak into public DAS headers.
