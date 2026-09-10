# Timers and PWM

DAS exposes timer functionality through generic opaque handles. Application code does not configure STM32 TIM instances, prescalers, auto-reload values, RCC enable bits, or output-compare fields directly.

## Periodic timer

The current STM32H755 baseline provides one general-purpose periodic timer backed internally by TIM2.

```c
const das_timer_config_t config = {
    .frequency_hz = 1000u,
};

das_timer_t timer = DAS_TIMER_INVALID;
if (das_timer_init(&config, &timer) != DAS_OK) {
    /* handle failure */
}

(void)das_timer_start(timer);
```

`frequency_hz` describes the requested update-event rate. The STM32 backend derives the live timer kernel clock from the existing DAS clock model and the RCC APB/TIMPRE state, then selects the timer period without requiring the application to calculate PSC/ARR values.

`das_timer_get_frequency()` returns the effective rate represented by the live timer registers and clock tree.

The baseline API also exposes start/stop state, counter read/reset, update pending/clear, update-source enable, and resolution of the generic `das_irq_t` used by the update interrupt. The application still owns handler/vector policy; #10 does not invent runtime callback registration merely to hide an ISR name.

## PWM

PWM is configured through a generic `das_pwm_t` and a board semantic route:

```c
const das_pwm_config_t config = {
    .frequency_hz = 1000u,
    .duty_per_mille = 500u,
};

das_pwm_t pwm = DAS_PWM_INVALID;
if (das_board_pwm_init(DAS_BOARD_PWM_ARDUINO_D4, &config, &pwm) != DAS_OK) {
    /* handle failure */
}

(void)das_pwm_start(pwm);
(void)das_pwm_set_duty(pwm, 250u); /* 25% */
```

Duty is represented as integer per-mille (`0..1000`) so the API does not require floating point while retaining 0.1% resolution.

For the NUCLEO-H755ZI-Q:

```text
DAS_BOARD_PWM_ARDUINO_D4 -> PE14 -> TIM1_CH4 / AF1 internally
```

The TIM1/AF1 details are board/backend facts. Application code selects the semantic D4 PWM resource.

## Timer clocks

STM32 timer clocks are not always equal to their APB bus clock. The backend derives the effective timer kernel frequency from the live AHB/APB frequencies and `RCC_CFGR.TIMPRE` behavior. In the qualified 400 MHz CM7 board profile, the APB buses run at 100 MHz while TIM1/TIM2 receive a 200 MHz timer kernel clock with the current TIMPRE/APB settings.

The CM4 focused image runs after reset and therefore independently verifies the same calculations against its live clock state rather than inheriting a compile-time constant.

## Focused physical qualification

Before timer/PWM is promoted into the full regression campaign, run:

```bash
./scripts/stm32h755_timer_test.sh /home/dev/STM32Cube/Repository/STM32CubeH7/
```

Connect one jumper:

```text
CN10 D4 / PE14 PWM output -> CN10 D3 / PE13 GPIO input
```

The focused qualifier runs separately on CM7 and CM4. Each image checks:

- a 1 kHz periodic timer derived from the live clock model;
- timer start/stop and counter state;
- TIM2 update events delivered through the generic DAS IRQ-controller API;
- 100 consecutive update periods measured with DWT cycles within 5%;
- a 1 kHz PWM output on D4;
- physical D4-to-D3 observation of 25%, 50%, and 75% duty cycles;
- PWM frequency/duty readback and continued execution.

The D3 GPIO observation deliberately keeps input-capture outside this baseline. Input capture can be added as an explicit timer capability later rather than appearing accidentally because the qualification fixture happened to need a measuring instrument.
