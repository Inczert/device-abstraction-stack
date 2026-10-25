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
The STM32H755 backend pulses `RCC_RSR.RMVF` and then explicitly
deasserts it, rather than keeping the reset-flags-clear input asserted.
The test records the raw `RCC_RSR` value even on a failed reset
assertion to aid diagnosis.

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

The single runner includes **two isolated two-reset hardware cases**, one on each core:
Each core explicitly clears old flags and triggers a software reset. On the
second boot it must detect the software-reset flag (but not IWDG1),
clear it, start its IWDG (IWDG1 on CM7, IWDG2 on CM4) for nominal 1200 ms and intentionally stop
feeding. On the third boot, it must detect that core's IWDG flag (but not the cleared
software flag), clear it and feed the watchdog while GDB captures the
result. Two SRAM `.noinit` markers keep the state across both resets.

A PASS therefore distinguishes software and IWDG reset origins on real
silicon, without manufacturing a reset flag in software. The test
requires no extra operator script, board jumper or button press.

**Current expected campaign target: 49 PASS / 0 FAIL.** That is a
requirement, not evidence it has been tested.

The CM4 case requires its firmware to restart after each reset. A board
with `BOOT_CM4=0` cannot pass this fixture unless CM7 explicitly releases
CPU2; the tested NUCLEO has `BOOT_CM4=1`. No test modifies option bytes.
The campaign restores its normal CM7 test image after the IWDG1 case
before executing CM4's IWDG2 case, so the two destructive test images
cannot independently reset the target at the same time.

Both cases remain **unqualified until the full 49-case physical campaign
passes on the exact PR head**.

## Reset-fixture diagnostics (October 2026)

The physical fixture distinguishes four persistent `.noinit` phases:
initial, software-reset requested (`SOFT`), IWDG configuration in progress
(`CONF`), and successfully armed (`WDG1`). A reset while the driver
is still configuring is **not** counted as a successful watchdog test.
It records the observed RCC reset flags, previously confirmed software
reset flags, restart count and read-only `FLASH_OPTSR_CUR` option status,
including whether the selected IWDG has hardware automatic start.

Since IWDG may start automatically through option bytes, each boot feeds
it at application entry before checking reset history. The production
watchdog start also refreshes before waiting on LSI register updates.
These changes do not relax the pass criteria: a successful two-reset case
still needs one software reset, a separate watchdog timeout reset, an
explicit flag-clear check, and three observed firmware boots. The
normal campaign never changes option bytes.

## Focused fault diagnosis (not release qualification)

The same top-level campaign also supports a development-only fast path:

```bash
./scripts/stm32h755_test_campaign.sh /path/to/STM32CubeH7 \\
  --watchdog-debug --clean --eth-iface <existing-host-interface>
```

This builds both cores and runs **only the two watchdog/reset-cause cases**
(CM7 then CM4), skipping UART/SPI/I2C/GPIO/Ethernet and interactive wiring.
It still produces one timestamped evidence archive with per-core debug logs
and a SHA-256 checksum. **2/2 PASS here is not a release gate:** the normal
`--release --clean` run still requires all 49 cases at the final SHA.
The debug mode cannot be combined with `--release`.

## Debugger freeze in destructive tests

STM32H755 hardware provides per-core IWDG debugger-freeze controls.
The destructive hardware test disables the current core's IWDG debug
freeze on entry and records the freeze register before/after, RCC LSI
clock status, IWDG prescaler/reload/status and FLASH watchdog option
bits. No option bytes are changed. This is test-only behavior: a DAS
production driver must not silently modify the application's debug
configuration. A timeout under an attached debugger is not considered
qualified unless actual reboot and hardware reset flags are observed.
