# Clock control

DAS owns the STM32H755 clock transition instead of relying on a CubeMX-generated `SystemClock_Config()`.

CMSIS device register definitions remain the low-level vocabulary. HAL/LL and generated Cube projects are not required.

## Public API

Normal applications select a standard frequency, not PLL dividers:

```c
#include <das/clock.h>

das_result_t result = das_clock_set_frequency(400000000u);
```

The selected board advertises its supported values:

```c
uint32_t frequencies[8];
size_t count = das_clock_get_supported_frequencies(frequencies, 8);
```

or a caller can query one value directly:

```c
if (das_clock_frequency_supported(300000000u)) {
    (void)das_clock_set_frequency(300000000u);
}
```

`das_clock_get_frequency()` returns the live primary/system frequency after the transition.

The public contract intentionally contains no STM32 PLL M/N/P/Q/R values, voltage-scale selections, FLASH wait states, RCC register fields, or board power-supply settings. Those are backend problems.

On a multi-core device the requested value is the primary/system frequency. Secondary-core and bus clocks can be derived from it and may run more slowly.

## NUCLEO-H755ZI-Q standard profiles

The board currently exposes:

```text
64 MHz
200 MHz
300 MHz
400 MHz
```

The initial profiles use the internal 64 MHz HSI as their source. This keeps frequency selection independent of ST-LINK MCO configuration while the board clock-source policy is still being expanded.

The 200/300/400 MHz profiles use PLL1 internally. DAS selects the PLL and bus dividers automatically.

The default NUCLEO-H755ZI-Q hardware uses the STM32H755 direct-SMPS core-supply path. DAS therefore configures direct SMPS before changing voltage scaling. This is a physical board constraint, not something application code should have to know.

The STM32H755 silicon can operate faster than 400 MHz, but the stock NUCLEO-H755ZI-Q direct-SMPS configuration is intentionally limited to 400 MHz. A 480 MHz CPU1 profile requires an LDO power path and corresponding board hardware configuration, so `480000000` is not advertised or accepted by the stock-board backend.

## Layer ownership

```text
application
    |
    v
das_clock_set_frequency()
    |
    v
NUCLEO-H755ZI-Q profile policy
src/board/nucleo_h755zi_q/clock.c
    |
    +-- supported standard frequencies
    +-- direct-SMPS board policy
    +-- profile -> device configuration
    |
    v
STM32H755 device clock/power engine
src/device/stm32h755/
    |
    +-- RCC
    +-- PWR
    +-- FLASH
    |
    v
CMSIS device definitions
```

This split matters because the STM32H755 does not know whether a board is wired for direct SMPS, LDO, an HSE crystal, ST-LINK MCO, or an external clock input. Device code knows how to operate the silicon; board code chooses the physically valid policy.

## Power-supply startup requirement

STM32H755 starts after POR in a limited Run* state. Before changing VOS, firmware must select a power-supply configuration and wait until `ACTVOSRDY` indicates a valid core supply.

Because DAS supplies its own startup rather than using ST's `system_stm32h7xx.c`, DAS must perform that step itself.

For the stock NUCLEO-H755ZI-Q the board layer selects direct SMPS. The device helper:

1. accepts the reset/unlocked Run* supply state;
2. transitions it to direct SMPS;
3. refuses to rewrite an already locked incompatible supply configuration;
4. waits for `PWR_CSR1_ACTVOSRDY` with a bounded timeout.

Refusing an incompatible locked state is deliberate. Applying a power configuration that does not match the physical board can make subsequent debug access fail.

The failed campaign from `dcff1e86fde3759b3352c7e158421e6be7e9bd2c` exposed exactly this missing startup step: the test remained at the 64 MHz reset clock and returned `DAS_ERROR_TIMEOUT` before any bus dividers were changed.

## CPU ownership

STM32H755 system clocks are shared silicon resources. The current policy is:

- CM7 / CPU1 may apply a global clock configuration;
- CM4 / CPU2 may query the effective tree but frequency changes return `DAS_ERROR_UNSUPPORTED`;
- production CM7-to-CM4 lifecycle and shared-clock coordination remain part of #20.

Two cores independently rewriting one PLL would be technically possible in the same sense that putting two steering wheels in a car is technically possible.

## Device clock engine

The private STM32H755 configuration supports:

- HSI;
- HSE crystal/resonator input;
- HSE bypass input;
- PLL1;
- D1/AHB/APB prescalers;
- VOS1;
- FLASH read latency and programming delay;
- bounded oscillator/power/clock-switch waits;
- live clock-tree readback.

For HSE, device code must be told the physical external source frequency. RCC registers can report that HSE is selected but cannot identify what frequency exists on the pin.

The current managed envelope is:

```text
CM7 / D1 core      <= 400 MHz
HCLK / CM4         <= 200 MHz
APB1..4            <= 100 MHz
voltage scale      = VOS1
FLASH latency      = 4 wait states
```

The public standard profiles currently keep VOS1 for all selectable frequencies. This favors simple, safe transitions over power optimization. Per-profile voltage scaling can be added later without changing the application API.

## Safe transition sequence

After the board power path is valid, the device clock engine:

1. validates source, PLL and divider values;
2. switches SYSCLK to HSI;
3. requests VOS1 and waits for `VOSRDY`;
4. raises FLASH latency/programming delay before increasing frequency;
5. installs safe D1/AHB/APB divisors;
6. configures the selected oscillator;
7. disables/reconfigures/restarts PLL1 when required;
8. switches SYSCLK to the requested source;
9. executes DSB/ISB barriers;
10. derives the effective tree from live RCC state and verifies the result.

All hardware waits are bounded. A transition that does not complete returns `DAS_ERROR_TIMEOUT`.

## Live readback

The internal device readback derives:

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

from the actual RCC source, PLL and prescaler registers.

Peripheral kernel clocks are intentionally handled with the peripheral that owns them. UART, SPI and timers have dedicated muxes and special rules, so pretending every peripheral clock is merely its APB frequency would be a charming source of future bugs.

## Hardware qualification

The dedicated CM7 clock image now exercises the public frequency-selection path rather than a raw PLL fixture.

It must:

- enumerate exactly the four stock-board profiles;
- reject 480 MHz;
- transition through 64, 200, 300 and 400 MHz;
- query each requested system frequency successfully;
- finish at 400 MHz;
- derive 400 MHz CM7, 200 MHz HCLK/CM4 and 100 MHz APB1..4 from live RCC state;
- confirm direct SMPS, `ACTVOSRDY` and `VOSRDY`;
- continue executing after the transitions.

The normal GPIO/IRQ images are then reloaded, keeping clock qualification isolated from the rest of the campaign.

The host-only linker/layout checks at the beginning of the campaign do not require a connected board. They are valid static tests and may pass even when hardware is absent; hardware execution starts with the OpenOCD probes.
