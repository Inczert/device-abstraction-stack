# DMA and data-cache coherency

DAS exposes generic DMA through an opaque execution-resource handle. Applications do not select STM32 DMA streams, DMAMUX channels, RCC bits or interrupt numbers.

The STM32H755 target currently has **two distinct DMA domains**:

```text
generic das_dma_t / SPI DMA -> DMA1 + DMAMUX1
Ethernet raw TX/RX          -> Ethernet peripheral DMA descriptor engine
```

The Ethernet engine is not routed through `das_dma_t`.

## Generic DMA API

A transfer is described in terms of logical source and destination:

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

`count` is a number of configured transfer elements, not bytes. The current generic STM32H755 backend uses direct mode, supports equal byte/halfword/word source/destination widths and bounds one hardware transfer to 65535 elements.

`das_dma_get_state()` distinguishes idle, busy, complete and terminal hardware-error states. `das_dma_get_remaining()` exposes the remaining element count. A finite wait uses the DAS monotonic time source and aborts the stream with `DAS_ERROR_TIMEOUT` when its deadline expires.

`das_dma_get_irq()` resolves the stream's generic `das_irq_t`. The current generic baseline qualifies completion through polling; no generic callback framework is implied.

## STM32H755 generic backend

The generic backend owns DMA1 streams 0..7 and corresponding DMAMUX1 channels. Allocation is local to one executing core; it is not a production cross-core resource arbiter.

The SPI DMA path uses private SPI1 RX/TX DMAMUX requests and two implementation-selected streams. Application code remains:

```c
das_spi_transfer_dma_timeout(spi, tx, rx, size, 50u);
```

## Ethernet DMA

STM32H755 Ethernet contains its own DMA engine and descriptor rings. DAS Ethernet therefore manages:

- four TX descriptors + internal TX buffers;
- four RX descriptors + internal RX buffers;
- descriptor ownership transitions;
- descriptor/buffer cache maintenance;
- DMA tail/ring progression.

Those objects are private to the Ethernet backend and never appear as `das_dma_t` resources.

Each software Ethernet descriptor occupies one 32-byte CM7 cache line and the hardware descriptor stride is configured to match. This prevents two independently owned descriptors from sharing a cache line.

See [Ethernet](ethernet.md).

## Cache coherency

DMA engines and Cortex-M7 D-cache are independent observers of memory. The Cortex-M layer exposes:

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

On CM7, range helpers expand arbitrary byte ranges to complete 32-byte cache lines before invoking CMSIS-Core cache maintenance. CM4 has no D-cache: range maintenance is a successful no-op, while enabling/disabling a nonexistent cache returns `DAS_ERROR_UNSUPPORTED`.

### Generic caller-owned DMA buffers

Generic DMA and SPI-DMA do **not** infer arbitrary caller-buffer ownership. Before DMA reads CPU-modified cacheable memory, clean it. Before/after DMA writes a cacheable destination, prepare/invalidate it according to the ownership transition.

Because maintenance works on complete cache lines, DMA destination buffers should be aligned/isolated when unrelated writable data could otherwise share the first or last line.

### Ethernet private buffers

The Ethernet backend owns its descriptor and frame buffers and therefore owns their cache transitions internally. Callers of `das_eth_send()` / `das_eth_receive()` do not manipulate Ethernet descriptor cache state.

## DMA-visible memory

The current default layouts place normal writable data in:

```text
CM7 .data/.bss -> AXI SRAM, 0x24000000...
CM4 .data/.bss -> D2 SRAM1, 0x30000000...
```

Both are appropriate for the standing generic-DMA tests. The CM7 AXI SRAM placement is also accessible by Ethernet DMA. DTCM is not a valid location for Ethernet descriptors/buffers.

Custom linkers must verify visibility for the specific DMA engine involved.

## Hardware qualification

Focused generic DMA qualification remains available:

```bash
./scripts/stm32h755_dma_test.sh /home/dev/STM32Cube/Repository/STM32CubeH7/
```

It reuses:

```text
Arduino D11 / PB5 / SPI1_MOSI  <->  Arduino D12 / PA6 / SPI1_MISO
```

Each core qualifies:

- generic DMA allocation/configuration/release;
- generic DMA IRQ resolution;
- 256-byte memory-to-memory integrity;
- completion/error state and zero remaining elements;
- 192-byte full-duplex SPI1 DMA transfer at 4 MHz;
- physical MOSI-to-MISO equality;
- cache behavior appropriate to that core.

The standing **39/39 PASS** STM32H755 campaign at DAS commit `f6b65672d9ae69cf28cd574d0dbba01cf875d8dc` includes both generic DMA/cache cases and the separate CM7 Ethernet DMA/cache data path.
