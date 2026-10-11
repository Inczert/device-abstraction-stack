# Device layer

`src/device/` contains silicon/device-specific implementation.

The current STM32H755 implementation includes GPIO/EXTI, RCC/PWR/FLASH clock control, USART, DMA/DMAMUX, timers/PWM, SPI, I2C and Ethernet MAC/PHY management. Production dual-core boot/HSEM/shared-memory control, ADC, watchdog and internal flash/reset-cause services belong at this layer **when implemented**; they are not part of DAS v0.1.0.

Device code may use the selected CMSIS device header internally. It must not encode NUCLEO connector wiring or other board-specific assumptions, and vendor types must not leak into public DAS headers.
