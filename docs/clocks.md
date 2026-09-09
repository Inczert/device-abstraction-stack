# STM32H755 clock control

DAS owns the STM32H755 clock transition instead of relying on a CubeMX-generated `SystemClock_Config()`.

This is device-layer functionality. CMSIS device register definitions remain the low-level vocabulary; HAL/LL and generated Cube projects are not required.

## Ownership

```text
application
    |
    v
board clock profile                 future #7
    |
    v
STM32H755 device clock engine       src/device/stm32h755/
    |
    +-- RCC
    +-- PWR voltage scale
    +-- FLASH latency/program delay
    |
    v
CMSIS device definitions
```

The device engine intentionally does not know whether a NUCLEO board has an HSE crystal, an ST-LINK clock input, a bypass source, or a particular jumper position. Those are board facts and belong to the board profile layer.

The current device configuration type is private to the STM32H755 backend. Normal applications are not expected to choose raw PLL M/N/P/Q/R values. Public, convenient board profiles are tracked separately in #7.

## CPU ownership

STM32H755 system clocks are shared silicon resources. The current policy is:

- CM7 / CPU1 may apply a global clock configuration;
- CM4 / CPU2 may read the effective tree but `stm32h755_clock_apply()` returns `DAS_ERROR_UNSUPPORTED`;
- production CM7-to-CM4 lifecycle and shared-clock coordination remain part of the dual-core work in #20.

This prevents two independently executing cores from casually reprogramming the same PLL because apparently a dual-core microcontroller did not already contain enough opportunities for excitement.

## Supported sources

The initial engine supports:

- HSI, fixed at 64 MHz;
- HSE crystal/resonator input;
- HSE bypass input;
- PLL1 as the system-clock source.

For HSE, the caller must supply the physical source frequency. RCC registers indicate that HSE is selected, but they cannot tell software what oscillator was soldered onto the board.

The accepted HSE input range is currently 4 to 48 MHz.

## Managed operating envelope

The first implementation deliberately supports a conservative STM32H755 performance envelope rather than attempting every legal voltage/frequency combination at once:

```text
CM7 / D1 core      <= 400 MHz
HCLK / CM4         <= 200 MHz
APB1..4            <= 100 MHz
voltage scale      = VOS1
FLASH latency      = 4 wait states
FLASH write delay  = 185..225 MHz HCLK range
```

Configurations outside this envelope return `DAS_ERROR_UNSUPPORTED` or `DAS_ERROR_INVALID_ARGUMENT` rather than being applied optimistically.

## Safe transition sequence

For a CM7 clock change the backend:

1. validates source, PLL and divider values;
2. switches SYSCLK to HSI and waits for the switch to complete;
3. requests VOS1 and waits for `VOSRDY`;
4. raises FLASH read latency and programming delay before increasing frequency;
5. installs conservative D1/AHB/APB divisors;
6. starts the selected oscillator;
7. disables/reconfigures/restarts PLL1 when requested;
8. switches SYSCLK to the requested source and waits for status confirmation;
9. executes DSB/ISB barriers;
10. derives the effective tree from the live RCC registers and verifies it matches the requested configuration.

Every oscillator/power/switch wait is bounded by `wait_limit`. A transition that never reaches its ready state returns `DAS_ERROR_TIMEOUT` instead of hanging forever.

`DAS_ERROR_TIMEOUT` is a common DAS result because future peripheral operations will need the same failure semantics.

## Clock readback

The internal readback returns:

```text
SYSCLK
CM7 / D1 core clock
CM4 clock
AHB / HCLK
APB1
APB2
APB3
APB4
```

The values are derived from the live source, PLL and prescaler registers, not copied from the requested configuration.

Peripheral kernel clocks are intentionally not generalized prematurely. UART, SPI, timers and other peripherals can have dedicated RCC muxes and special rules. Their effective kernel clocks should be added alongside those device drivers rather than pretending every peripheral is simply its APB clock.

## Hardware qualification profile

Issue #6 uses an HSI-only high-performance profile so device-layer qualification does not depend on NUCLEO-specific HSE wiring:

```text
source          HSI 64 MHz
PLL1 M          8
PLL1 reference  8 MHz
PLL1 N          100
VCO             800 MHz
PLL1 P          2
SYSCLK / CM7    400 MHz
D1 prescaler    /1
AHB prescaler   /2
HCLK / CM4      200 MHz
APB1..4         /2
APB1..4         100 MHz
```

The campaign uses a separate CM7 clock-test image. It applies the profile, continues executing at the new frequency, reads the live tree back, and requires the exact expected frequencies. The target is reset before the normal GPIO/IRQ campaign, keeping clock qualification isolated from unrelated tests.

The board-specific HSE/bypass profile comes next in #7.
