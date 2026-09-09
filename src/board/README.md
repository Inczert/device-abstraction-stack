# Board layer

`src/board/` contains physical board mappings and semantic resources.

Examples include LEDs, buttons, connector buses, virtual COM mappings, board sensors, fixed enables/chip-selects, and physical clock-source wiring.

Board code should consume public/device capabilities rather than duplicate register programming. It should not become a one-for-one alias table for every MCU pin.
