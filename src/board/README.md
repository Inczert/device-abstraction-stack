# Board layer

`src/board/` contains physical board mappings and semantic resources.

For the current NUCLEO-H755ZI-Q implementation this includes LEDs, the B1 button, ST-LINK/Arduino UART routing, Arduino SPI/I2C/PWM connections, GPIO fixture pins, Ethernet PHY/RMII/RJ45 routing and board clock/power policy. Sensor-specific board APIs are not part of the v0.1.0 implementation; potential future boards can add them when the relevant sensors exist and are qualified.

Board code should consume public/device capabilities rather than duplicate register programming. It should not become a one-for-one alias table for every MCU pin.
