# Timers and PWM

DAS exposes timer functionality through generic opaque handles. Application code does not configure STM32 TIM instances, prescalers, auto-reload values, RCC enable bits or output-compare fields directly.

## Periodic timer

The current STM32H755 baseline provides one general-purpose periodic timer backed internally by TIM2.

```c
const das_timer_config_t config = {
    .frequency_hz = 1000u,
};

das_timer_t timer = DAS_TIMER_INVALID;
(void)das_timer_init(&config, &timer);
(void)das_timer_start(timer);
```

The API exposes:

- requested/effective periodic frequency;
- start/stop/running state;
- counter read/reset;
- update-event source enable/pending/clear;
- generic `das_irq_t` resolution.

The final application still owns the concrete vector/handler binding.

## PWM

```c
const das_pwm_config_t config = {
    .frequency_hz = 1000u,
    .duty_per_mille = 500u,
};

das_pwm_t pwm = DAS_PWM_INVALID;
(void)das_board_pwm_init(DAS_BOARD_PWM_ARDUINO_D4, &config, &pwm);
(void)das_pwm_start(pwm);
```

Duty uses integer per-mille `0..1000`, providing 0.1% resolution without requiring floating point.

Current route:

```text
DAS_BOARD_PWM_ARDUINO_D4 -> PE14 -> TIM1_CH4 / AF1 internally
```

The API supports start/stop/running state, set/get duty and effective-frequency query.

## Timer clocks

The STM32H755 backend derives timer kernel frequency from the live DAS clock tree plus APB/TIMPRE behavior. It does not assume timer clock equals APB clock. In the qualified 400 MHz CM7 profile, the current APB/TIMPRE configuration yields a 200 MHz TIM1/TIM2 kernel clock.

## Hardware qualification

The focused qualifier remains available:

```bash
./scripts/stm32h755_timer_test.sh /home/dev/STM32Cube/Repository/STM32CubeH7/
```

Fixture:

```text
CN10 D4 / PE14 PWM output <-> CN10 D3 / PE13 GPIO input
```

Each core verifies a 1 kHz periodic timer, start/stop/counter behavior, TIM2 update delivery through the generic IRQ API, 100 measured periods within 5%, a 1 kHz PWM output, physical observation at 25/50/75% duty, readback and continued execution.

Timer input capture is intentionally outside the current baseline.

Timer/PWM is already promoted into the standing campaign and is included in the completed **38/38** STM32H755 regression baseline.
