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

## Qualification

The single `stm32h755_test_campaign.sh --release --clean` includes two
physical HSEM cases plus one **CM4 autonomous execution** case, for
**47 expected PASS** including four host contracts.

The autonomous fixture flashes the CM4 ELF into bank 2 using the CM7
OpenOCD connection, programs and resets the CM7 firmware, and resumes
**only CM7**. The CM7 firmware requests CM4 boot and publishes a
challenge in unused shared D2 SRAM3 at 0x30040000; CM4 acknowledges and
releases HSEM channel 30. CM7 checks the shared response, HSEM
release-notification, and clock-domain state. There is **no CPU2 GDB
resume**, no option-byte change and no extra wiring. Its GDB evidence logs the
current `BOOT_CM4` option bit (0 or 1), allowing the results to distinguish
a software-request-required startup from factory autoboot.

**Qualification is pending until the full campaign passes on real hardware.**
With BOOT_CM4 enabled at reset, CM4 may already be running before CM7
requests boot; the test proves autonomous CM4 operation but not that the
software boot-request bit was necessary. With BOOT_CM4 disabled and a
valid configured bank-2 boot address, the request should release CM4.
No automatic option-byte programming, core reset/stop, application IPC or
lifecycle supervision is added.
