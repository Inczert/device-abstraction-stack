# DAS examples

Every example in this directory is a standalone application CMake project. Normal examples consume a generated/installed DAS package with:

```cmake
find_package(DAS CONFIG REQUIRED)
target_link_libraries(application PRIVATE das::das)
```

They do not add the DAS source tree, include implementation headers, or carry private STM32 startup/linker files.

| Example | Purpose |
| --- | --- |
| `led_blink` | minimal green user-LED blink using the default DAS vector/startup path |
| `time_periodic` | board clock + default SysTick monotonic time + periodic LED work |
| `uart_console` | one-shot ST-LINK VCP UART output |
| `clock_uart` | set/query the board/core clock before configuring UART |
| `hardrt_uart` | HardRT scheduler with DAS clock/GPIO/UART and RTOS-backed DAS time |

`scripts/ci/build_installed_examples.sh` cross-builds all DAS-only examples against an installed CM7 or CM4 package. The HardRT example has its own CI job because it consumes a second installed library.
