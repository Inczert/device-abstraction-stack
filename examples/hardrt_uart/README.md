# HardRT + DAS UART example

This example demonstrates one ownership model for using [HardRT](https://github.com/ExoSpaceLabs/hardrt) 0.5.1 as the Cortex-M scheduler while DAS owns board clocking and peripherals.

Both dependencies are consumed as installed CMake packages:

```cmake
find_package(DAS CONFIG REQUIRED)
find_package(HardRT 0.5.1 CONFIG REQUIRED)
target_link_libraries(das_hardrt_uart PRIVATE HardRT::hardrt das::das)
```

The application uses the convenience umbrella header:

```c
#include <das/das.h>
```

`das/das.h` exposes the normal application-facing DAS API. Architecture-specific startup/vector customization remains explicit through `das/cortex_m/startup.h`.

## Initialization structure

The example keeps `main()` focused on orchestration. DAS clock, board LED and UART setup live in a local `das_init()` helper. RTOS setup lives in `rtos_init()`, including the DAS-to-HardRT monotonic-time bridge because that bridge can only be established once HardRT owns and initializes SysTick.

```text
das_init()
    -> DAS clock profile
    -> query live core frequency
    -> DAS board LED
    -> DAS ST-LINK UART

rtos_init()
    -> hrt_init(core_hz from DAS)
    -> das_time_set_source(hrt_now_ms)

main()
    -> das_init()
    -> rtos_init()
    -> create tasks
    -> hrt_start()
```

## Ownership

- DAS owns STM32H755/NUCLEO clock setup and UART/GPIO access.
- HardRT owns `SysTick_Handler`, `PendSV_Handler`, its diagnostic `HardFault_Handler`, the scheduler tick and context switching.
- DAS supplies the reset/runtime path, default vector table and linker layout.
- Strong HardRT core handlers replace the weak DAS defaults in that vector table.
- `das_time_init()` is deliberately **not** called. DAS time is redirected to `hrt_now_ms()` with `das_time_set_source()`.

The application sets the DAS 400 MHz board profile **before** initializing either UART or HardRT, queries the live CM7 core rate, and passes that exact value as `hrt_config_t.core_hz`. Changing the core clock after HardRT has configured SysTick would invalidate the RTOS tick period, and changing it after UART initialization would invalidate UART baud programming, so runtime clock changes require coordinated reconfiguration rather than a casual `das_clock_set_frequency()` call.

HardRT currently expects generic `__RAM_START__`/`__RAM_END__` linker symbols for task-stack validation. DAS exports those aliases from its default core-specific writable RAM region so the RTOS can use the DAS linker policy without vendor startup/linker files.

The example has one LED task and one UART task. Only the UART task owns the console; applications with multiple UART users should serialize access with an RTOS mutex or central I/O task because the current DAS UART API does not provide internal task-level locking.

## UART destination

The UART task initializes `DAS_BOARD_UART_STLINK_VCP`. On the NUCLEO-H755ZI-Q board mapping this resolves to USART3 on PD8 (TX) and PD9 (RX), routed through the on-board ST-LINK Virtual COM Port. The same USB connection used for ST-LINK/OpenOCD therefore also exposes the UART to the host as a serial device.

Runtime configuration:

```text
UART:    USART3
TX:      PD8
RX:      PD9
Host:    ST-LINK Virtual COM Port
Format:  115200 8N1
Message: HardRT + DAS alive\r\n
Period:  1 second
```

On Linux, prefer the stable `/dev/serial/by-id/...` symlink over assuming a particular `/dev/ttyACM0` number.

## Build and flash

When `hardrt/` and `device-abstraction-stack/` are sibling checkouts, the helper finds HardRT automatically:

```bash
./scripts/build_and_flash_hardrt_uart.sh \
    /home/dev/STM32Cube/Repository/STM32CubeH7/
```

Otherwise specify the checkout explicitly:

```bash
./scripts/build_and_flash_hardrt_uart.sh \
    /home/dev/STM32Cube/Repository/STM32CubeH7/ \
    --hardrt-root /path/to/hardrt
```

The helper performs the same external-consumer flow as CI:

1. cross-build and install `libdas.a` for STM32H755 CM7;
2. cross-build and install `libhardrt.a` for the Cortex-M port;
3. configure `examples/hardrt_uart` only through the generated DAS and HardRT CMake packages;
4. build `das_hardrt_uart.elf`;
5. flash CM7 with OpenOCD while keeping CM4 halted;
6. report the detected ST-LINK VCP device when available.

To attach a simple serial monitor immediately after flashing:

```bash
./scripts/build_and_flash_hardrt_uart.sh \
    /home/dev/STM32Cube/Repository/STM32CubeH7/ \
    --monitor
```

A serial device can also be supplied explicitly:

```bash
./scripts/build_and_flash_hardrt_uart.sh \
    /home/dev/STM32Cube/Repository/STM32CubeH7/ \
    --serial-port /dev/ttyACM0 \
    --monitor
```

At runtime the green LED changes state every 250 ms and the UART task writes `HardRT + DAS alive` once per second.

CI cross-links this example against current HardRT 0.5.1 and verifies that the final ELF contains HardRT's strong `HardFault_Handler`, `PendSV_Handler` and `SysTick_Handler` plus the DAS vector table.
