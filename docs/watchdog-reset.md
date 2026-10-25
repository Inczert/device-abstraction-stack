# Independent watchdog and reset causes (STM32H755)

DAS v0.2.0 issues #14 and #39 provide **hardware primitives only**:

- `das_watchdog_start()`: choose the executing core's independent watchdog
  (CM7 → IWDG1, CM4 → IWDG2) and program a /4 to /256 prescaler plus a
  12-bit reload using caller-supplied timeout and estimated LSI frequency.
- `das_watchdog_feed()`: write the hardware refresh key. There is no
  scheduler callback, health-check policy, daemon or automatic feed.
- `das_reset_cause_get()` and `das_reset_cause_clear()`: obtain latched,
  **nonexclusive** reset flags and clear them only on explicit request.

## Irreversible behavior

An IWDG cannot be stopped once started, except by reset. **A TIMEOUT result
after `das_watchdog_start()` is not a rollback**; firmware must assume the
watchdog might still be active. Invalid configurations are rejected before
starting it. Hardware-start option bytes can start watchdogs without a call
to DAS, and `feed()` remains available to the application after a reset.

The IWDG uses an uncalibrated independent LSI oscillator. `lsi_hz` is the
application's measured/estimated LSI frequency (the test uses 32000 Hz),
not an accuracy guarantee. Requested timeouts are rounded to representable
reload intervals. The actual physical timeout varies with LSI tolerance.

## Shared RCC reset flags

Multiple reset causes may be asserted simultaneously: on STM32H7 an IWDG
reset may also assert the external/pin flag. DAS never normalizes them
into a single guessed cause. Both cores see the shared RCC reset flags.
Applications must establish **one owner at early boot** to capture and,
later, explicitly clear flags; otherwise CM4 may erase evidence before CM7
reads it. Startup and board initialization do not clear reset flags.

## Example

```c
das_reset_cause_t cause = {0};
if (das_reset_cause_get(&cause) == DAS_OK) {
    // Persist/record the cause in application storage first.
    (void)das_reset_cause_clear();
}

das_watchdog_config_t watchdog = {
    .timeout_ms = 1200,
    .lsi_hz = 32000,
    .update_poll_limit = 1000000
};
uint32_t nominal_ms = 0;
das_result_t result = das_watchdog_start(&watchdog, &nominal_ms);
if (result == DAS_OK) {
    // The application or HardRT alone decides when it is safe to feed.
    (void)das_watchdog_feed();
}
```

## Single-campaign qualification

The mandatory command remains:

```bash
./scripts/stm32h755_test_campaign.sh /path/to/STM32CubeH7 \
  --release --clean --eth-iface <wired-linux-interface>
```

The single runner now includes an **isolated destructive IWDG1** case:
a CM7 test clears reset flags, starts IWDG1 for nominal 1200 ms, stops
feeding and waits for the MCU to reset. On the next boot, AXI SRAM
`.noinit` state identifies the pending test; RCC must report the IWDG1
reset, and explicit flag clearing must remove that latch. No extra
operator script, board jumper or button press is required.

**Current expected campaign target: 48 PASS / 0 FAIL.** That is a
requirement, not evidence it has been tested.

The CM4 IWDG2 firmware cross-builds, but the CM4 destructive reboot path
has not yet been physically tested and must not be described as qualified.
