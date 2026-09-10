# LED blink external-consumer example

This example deliberately builds as a separate CMake project. It does **not** use `add_subdirectory()` on the DAS source tree.

The producer side builds and installs the static `libdas.a`, public headers, CMake package metadata and the selected linker script. This application then resolves that generated installation with:

```cmake
find_package(DAS CONFIG REQUIRED)
target_link_libraries(das_led_blink PRIVATE das::das)
```

For the NUCLEO-H755ZI-Q CM7 example, the application initializes the semantic green user LED, initializes the DAS SysTick time source and toggles the LED every 500 ms.

From the repository root, build, install, consume, flash and visually verify it with:

```bash
./scripts/build_and_flash_led_blink.sh /path/to/STM32CubeH7
```

The helper uses the repository's ARM GCC toolchain and qualified direct-DAP OpenOCD configuration. The application CMake project itself only consumes the installed DAS package.
