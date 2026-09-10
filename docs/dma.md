# DMA and data-cache coherency

DAS exposes DMA through an opaque execution-resource handle. Applications do not select STM32 DMA streams, DMAMUX channels, RCC bits or interrupt numbers. The STM32H755 baseline uses DMA1 internally and allocates an available stream when `das_dma_acquire()` is called.

## Generic DMA API

A transfer is described in terms of logical source and destination rather than STM32's peripheral/memory register naming:

```c
das_dma_t dma = DAS_DMA_INVALID;
das_dma_acquire(&dma);

const das_dma_config_t config = {
    .direction = DAS_DMA_MEMORY_TO_MEMORY,
    .source_width = DAS_DMA_WIDTH_BYTE,
    .destination_width = DAS_DMA_WIDTH_BYTE,
    .source_increment = true,
    .destination_increment = true,
};

das_dma_configure(dma, &config);
das_dma_start(dma, source, destination, count);
das_dma_wait_timeout(dma, 50u);
das_dma_release(dma);
```

`count` is a number of configured transfer elements, not a byte count. The first STM32H755 baseline uses direct mode, supports byte/halfword/word elements with equal source and destination widths, and bounds one hardware transfer to 65535 elements.

`das_dma_get_state()` distinguishes idle, busy, complete and terminal hardware-error states. `das_dma_get_remaining()` exposes the remaining element count. On STM32H755, a transfer error (`TEIF`) maps to `DAS_DMA_STATE_ERROR` and `DAS_ERROR_IO`. FIFO/direct-mode back-pressure flags (`FEIF`/`DMEIF`) do not by themselves terminate the stream, so completion remains authoritative unless a terminal DMA or peripheral error occurs. A finite wait uses the generic DAS monotonic time source; it aborts the stream and returns `DAS_ERROR_TIMEOUT` when the deadline expires.

`das_dma_get_irq()` resolves the stream's generic `das_irq_t` without exposing STM32 IRQ types. Interrupt-driven transfer ownership/callback policy can build on this primitive later; the first baseline qualifies completion through polling so no backend ISR contract is invented prematurely.

## STM32H755 backend

The current backend owns DMA1 streams 0..7 and their corresponding DMAMUX1 channels. The stream allocator is intentionally local to one executing core. It is not yet a cross-core resource arbiter; production CM7/CM4 ownership and HSEM coordination belong with #20.

For memory-to-memory transfers the DMAMUX request is zero. Peripheral drivers may select a private device request internally. Raw DMAMUX request identifiers are never part of `das/dma.h`.

The current SPI DMA path uses the STM32H755-private SPI1 RX/TX requests and two implementation-selected DMA streams. Application code remains:

```c
das_spi_transfer_dma_timeout(spi, tx, rx, size, 50u);
```

The existing MOSI/MISO board route and SPI framing/clock setup are reused unchanged.

## Cache coherency is explicit

DMA engines and Cortex-M7 D-cache are independent observers of memory. A DMA API that silently pretends otherwise eventually produces a bug whose most endearing property is intermittency.

DAS therefore does **not** perform cache maintenance automatically in generic DMA or SPI-DMA calls. The Cortex-M layer exposes:

```c
das_cache_data_available();
das_cache_data_is_enabled();
das_cache_data_line_size();
das_cache_data_enable();
das_cache_data_disable();
das_cache_data_clean(ptr, size);
das_cache_data_invalidate(ptr, size);
das_cache_data_clean_invalidate(ptr, size);
```

On Cortex-M7 the range helpers expand an arbitrary byte range to complete 32-byte cache lines before calling CMSIS-Core cache maintenance primitives. On the current Cortex-M4 target there is no D-cache: range maintenance is a successful no-op, while trying to enable/disable a nonexistent D-cache returns `DAS_ERROR_UNSUPPORTED`.

### DMA reads memory: CPU -> DMA

Before DMA reads a buffer that the CPU may have modified in cache, clean it:

```c
das_cache_data_clean(tx, tx_size);
```

This writes dirty cache lines back to DMA-visible SRAM.

### DMA writes memory: DMA -> CPU

Before a DMA destination is handed to hardware, make sure dirty CPU data cannot later overwrite DMA results. For an isolated DMA buffer:

```c
das_cache_data_clean_invalidate(rx, rx_size);
```

After DMA completes, invalidate before reading the result:

```c
das_cache_data_invalidate(rx, rx_size);
```

Because maintenance operates on whole cache lines, DMA destination buffers should be aligned and isolated at cache-line granularity whenever unrelated writable objects could otherwise share the first or last line. Invalidating a dirty shared line can discard unrelated CPU writes. The helper hides CMSIS alignment mechanics, not the ownership rule.

## DMA-visible memory

The STM32H755 DMA1 engine cannot access every CPU-local memory region. The current hardware qualification deliberately uses memory already selected by DAS linker policy:

```text
CM7 .data/.bss -> AXI SRAM, 0x24000000...
CM4 .data/.bss -> D2 SRAM1, 0x30000000...
```

Both are DMA-visible. Code that later places DMA buffers into TCM or another special region must verify that the chosen DMA engine can reach it. DAS does not turn an unreachable physical address into a reachable one by optimism.

## Focused qualification

Before DMA is promoted into the standing hardware campaign, run:

```bash
./scripts/stm32h755_dma_test.sh /home/dev/STM32Cube/Repository/STM32CubeH7/
```

The peripheral part reuses the qualified SPI loopback fixture:

```text
Arduino D11 / PB5 / SPI1_MOSI  <->  Arduino D12 / PA6 / SPI1_MISO
```

Each core runs a dedicated image. The focused qualifier checks:

- opaque DMA allocation/configuration/release;
- generic DMA IRQ resolution;
- 256-byte memory-to-memory integrity and completion state;
- zero remaining elements after completion;
- CM7 D-cache availability, 32-byte line size, enable state and explicit clean/invalidate path;
- CM4 no-cache behavior through the same range-maintenance API;
- 192-byte full-duplex SPI1 transfer through DMAMUX1/DMA1;
- explicit cache preparation/invalidation around the SPI buffers;
- exact physical MOSI-to-MISO equality;
- continued execution after the complete sequence.

CM7 first selects the qualified 400 MHz board profile. CM4 independently runs at its qualified 64 MHz reset profile. The focused test must pass on both cores before #9 is integrated into the main campaign.
