# Changelog

This file records user-visible DAS release changes. DAS follows semantic versioning intent, with the additional pre-1.0 rule that minor releases may intentionally evolve the public API.

## 0.1.0

First tagged DAS baseline for NUCLEO-H755ZI-Q / STM32H755.

### Added

- layered C11 architecture separating public APIs, Cortex-M support, STM32H755 device code, NUCLEO-H755ZI-Q board policy and linker/build policy;
- reusable weak Cortex-M reset/runtime support and canonical STM32H755 vector table with strong-handler override;
- CM7 and CM4 build/link/startup support with hard-float FPU enablement;
- default CM7/CM4 linker layouts plus custom linker override;
- device-neutral IRQ/NVIC control;
- 64/200/300/400 MHz clock profiles and power/FLASH sequencing;
- monotonic SysTick time source plus external/RTOS source injection;
- GPIO, pulls, output type/speed, alternate functions and EXTI;
- UART polling/blocking I/O;
- SPI modes 0..3, both bit orders, polling and full-duplex DMA;
- I2C 7-bit controller at 100/400 kHz;
- periodic timer and PWM control;
- generic DMA1/DMAMUX1 plus explicit CM7 cache-coherency helpers;
- CM7 polling Layer-2 Ethernet MAC/DMA/RMII support with LAN8742A link-state and raw TX/RX;
- semantic NUCLEO-H755ZI-Q board resources;
- relocatable CMake install/export package with `das::das`;
- target-specific CM7/CM4 installed-package metadata, license and build/ABI provenance;
- standalone installed-package examples and HardRT 0.5.1 consumer validation;
- reusable physical STM32H755 qualification campaign with evidence archives.

### Qualified baseline

The pre-release implementation baseline completed a 40/40 physical campaign on NUCLEO-H755ZI-Q, including both cores, real VFP execution, serial/peripheral loopbacks, timer/PWM, and Ethernet link-loss/recovery plus post-recovery traffic.

Before v0.1.0 is promoted to `main`, the final release-candidate content must pass the same complete campaign with exactly 40 passes, zero failures and exit code zero. Hardware qualification is intentionally manual; merging the qualified candidate to `main` is the release authorization. Successful `main` CI then creates the tag and launches packaging automatically.

### Known boundaries

- only STM32H755 / NUCLEO-H755ZI-Q is currently supported;
- Ethernet runtime ownership is CM7-only and polling Layer 2 only;
- CM4 physical execution is qualified under debugger control, but production CM7-to-CM4 boot/release, HSEM and shared-memory ownership are not yet implemented;
- ADC, watchdog, internal flash/reset-cause services and the optional C++ convenience layer are not part of 0.1.0;
- no HAL, LL, CubeIDE or CubeMX-generated source/startup/linker files are required for the supported path;
- CMSIS-Core and STM32H755 CMSIS device headers remain build dependencies and are not bundled.

### Packaging/versioning

The root `VERSION` file is the single source of truth used by CMake and release automation. DAS 0.x CMake packages use same-minor compatibility. A 0.1.x package may satisfy a 0.1 request but does not automatically satisfy a 0.2 request. Revisit this policy at v1.0 under issue #29.
