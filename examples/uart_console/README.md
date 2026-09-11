# UART console example

This is a standalone installed-package consumer using `find_package(DAS CONFIG REQUIRED)` and `das::das`.

It configures the semantic ST-LINK VCP UART for 115200 8N1, writes `DAS UART ready`, flushes the transmitter, and returns. No STM32 peripheral instance, alternate-function value, HAL/LL object, startup file, or linker script appears in the application source.

It is cross-built in CI for both CM7 and CM4 DAS packages.
