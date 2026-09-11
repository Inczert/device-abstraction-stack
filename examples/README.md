# DAS examples

The examples are standalone consumers of the installed DAS CMake package rather than source-tree-only demos. They are intended to show application structure without copying startup/vector boilerplate into each firmware.

Application code may include focused headers or the convenience umbrella:

```c
#include <das/das.h>
```

The umbrella exposes the normal application-facing DAS C API. Architecture-specific startup/vector customization remains explicit through headers such as `das/cortex_m/startup.h`.

Current examples:

- `led_blink` - minimal board LED + standalone DAS SysTick timebase;
- `time_periodic` - clock profile + monotonic periodic work;
- `uart_console` - ST-LINK VCP UART output;
- `clock_uart` - configure CPU clock before clock-dependent UART setup and inspect the effective values;
- `hardrt_uart` - HardRT 0.5.1 scheduler with DAS clock/GPIO/UART and the RTOS-provided monotonic time source.

The HardRT example also has `scripts/build_and_flash_hardrt_uart.sh` for building/installing both libraries, flashing CM7 and optionally monitoring the ST-LINK VCP.
