# STM32H755 secondary-core control and hardware semaphores

DAS v0.2.0 workstream #20 provides low-level **hardware-only** primitives:

- `das_secondary_core_request_boot()` (CM7 only) sets RCC's software
  `BOOT_C2` request. CM4 must already have a valid image at its configured
  boot address (default DAS linker image at `0x08100000`). It cannot force
  a running CM4 to reset or guarantee the CM4 main loop is executing.
  CM4 builds return `DAS_ERROR_UNSUPPORTED`.
- `das_secondary_core_get_state()` reads the RCC software request and D2
  clock-domain-ready bit, **not** application liveness. The core can already
  be running from its hardware BOOT_CM4 option-byte setting.
- `das_hsem_*` wraps fast single-step hardware semaphore lock/unlock
  with process ID 0, release-event notification and per-core HSEM IRQ query.
  Channel selection 0..31 is **application policy**, not DAS reservation.
  A failed take is nonblocking; no heap or scheduler integration.
  HSEM IRQ handling is owned by the consumer/RTOS; DAS does not install an ISR.

**CM7-only application remains valid**: neither function is invoked at startup,
and DAS never enables CPU2 implicitly. Configure BOOT_CM4 device option bytes
appropriately when CM4 must *not* boot at reset. Turning off a running core
is outside this API (and cannot be safely inferred from RCC boot gate bits).

## Hardware resource ownership

CM7 is system clock/power authority; CM4 never changes the global RCC/PLL
tree. Peripherals are shared silicon resources and must have a single
application-designated owner, including GPIO alternate-function routes, DMA
channels and NVIC handler selection. DAS does not arbitrate them
automatically.

Default non-overlapping runtime images:
- CM7 flash bank 1 `0x08000000`, AXI SRAM `0x24000000`.
- CM4 flash bank 2 `0x08100000`, D2 SRAM1 `0x30000000`.
- Shared D2 SRAM2/SRAM3 may be reserved with application linker scripts.
  No shared IPC structure or memory protocol is provided by DAS.

The CM7 D-cache can keep stale shared memory even while an HSEM lock
synchronizes writers. Synchronization must use proper barriers and,
where regions are cacheable, clean/invalidate on ownership transfer or
an MPU-configured noncacheable shared window. Neither `volatile` nor an
HSEM alone supplies inter-core cache coherence.

## Qualification and BOOT_CM4 profiles

The complete single-command STM32H755 `--release --clean` campaign
passed **49/49** on 2026-10-25 at DAS commit
`4da083a4bfbbdc7959f4c4dd9e96851c45079385`, with the board
option `BOOT_CM4=1`. It covers the two HSEM cases, autonomous CM4 execution,
the CM7/CM4 watchdog tests and the previous regressions. This is proof of
autonomous execution with CM4 *autoboot enabled*, not proof that a CM7
software request was necessary.

### Gated boot: optional focused physical qualification (#20)

The same dual-image firmware and GDB acceptance case now instrument a
pre-request observation window. The CM4 firmware announces that it reached
`main()` in a dedicated shared-memory marker before waiting for the
challenge, while CM7 deliberately withholds the request. If `BOOT_CM4=0`,
**CM4 must remain absent** during that window. CM7 then issues
`das_secondary_core_request_boot()` and requires CM4 to appear, answer the
shared-memory challenge, release HSEM and keep a heartbeat alive.

There are **two explicit, independent proof conditions**: no CM4 execution
before the boot request and successful CM4 execution after it. The GDB
script logs both, the actual `SYSCFG.UR1.BCM4` option-bit state, initial
RCC request state and final heartbeat.

The normal release campaign does **not** change option bytes, and still
expects **49** test passes. On a controlled bench with `BOOT_CM4=0`
preconfigured, run **development-only**:

```bash
./scripts/stm32h755_test_campaign.sh \
  /home/dev/STM32Cube/Repository/STM32CubeH7 \
  --cm4-boot-gated --clean --eth-iface enp0s31f6
```

It reuses the original top-level campaign, builds the two core images,
flashes CM4 into its expected bank-2 boot address and runs one GDB case.
It should yield **1 PASS / 0 FAIL** and a self-contained evidence archive.
It does not require UART/SPI/I2C/Ethernet wiring. It *rejects* the factory
`BOOT_CM4=1` setting, rather than silently counting autoboot as proof.
`--cm4-boot-gated` is mutually exclusive with `--release` and
`--watchdog-debug`, and **cannot substitute for the full release gate**.

### Option-byte safety and restoring the fixture

Changing `BOOT_CM4` is a nonvolatile hardware configuration operation,
not a routine software test. **The DAS scripts do not change option
bytes.** Only conduct this focused profile with an intentionally prepared
bench board and a known valid **CM4 boot address pointing at the CM4 image
(default `0x08100000`)**.

Use STM32CubeProgrammer or an equivalent supported provisioning method
to *read and save* the original option-byte configuration before any
manual changes. Record the original `BOOT_CM4` and CPU2 boot address,
then explicitly set **only `BOOT_CM4=0`** and apply a device reset or
power cycle as required by the programmer. Do not change CPU1 boot,
readout protection, write-protection or other options to satisfy this
test. The GDB case verifies the measured `BOOT_CM4` state on silicon
and refuses a false PASS.

After collecting the focused evidence, **restore the original option
bytes** (`BOOT_CM4=1` on the previously tested NUCLEO), re-read them to
verify and rerun the complete **49-case `--release --clean` campaign**
on the unchanged final source tree. Keep both profile archives and
their SHA-256 checksums. The gated-boot evidence qualifies software-only
CPU2 release; the full campaign checks there was no regression after
hardware configuration was restored.

No debugger ever resumes CPU2 to manufacture evidence, and DAS does not
provide a CM4 scheduler, IPC, shutdown/restart policy or application
heartbeat API.
