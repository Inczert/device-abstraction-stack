# Clock + UART example

This standalone installed-package consumer demonstrates the ordering needed when peripheral configuration depends on the live clock tree:

1. select the 400 MHz DAS board clock profile;
2. query the executing core frequency;
3. initialize the semantic ST-LINK VCP UART at 115200 8N1;
4. query the effective baud rate and transmit a short message.

The observed core and UART rates are kept in `g_das_example_core_hz` and `g_das_example_uart_baud_hz` for debugger inspection. The example uses no STM32 register types or vendor startup code.
