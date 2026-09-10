set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

monitor arm semihosting disable
monitor reset halt

printf "Flashing STM32H755 DMA/cache qualification image...\n"
load
compare-sections

# Stop at the first SPI-DMA cleanup before the backend clears the registers.
# This snapshot is diagnostic only; normal execution resumes immediately after it.
break dma_pair_cleanup
continue

set $rx_index=(unsigned int)rx_dma.storage
set $tx_index=(unsigned int)tx_dma.storage
set $rx_stream=0x40020010 + ($rx_index * 0x18)
set $tx_stream=0x40020010 + ($tx_index * 0x18)
set $rx_mux=0x40020800 + ($rx_index * 4)
set $tx_mux=0x40020800 + ($tx_index * 4)

printf "DMA_CLEANUP_SNAPSHOT rx_stream=%u tx_stream=%u\n", $rx_index, $tx_index
printf "SPI1 SR=0x%08x CR1=0x%08x CR2=0x%08x CFG1=0x%08x CFG2=0x%08x\n", (unsigned int)registers->SR, (unsigned int)registers->CR1, (unsigned int)registers->CR2, (unsigned int)registers->CFG1, (unsigned int)registers->CFG2
printf "DMA1 LISR=0x%08x HISR=0x%08x\n", *(unsigned int*)0x40020000, *(unsigned int*)0x40020004
printf "RX CR=0x%08x NDTR=%u PAR=0x%08x M0AR=0x%08x FCR=0x%08x DMAMUX=0x%08x\n", *(unsigned int*)$rx_stream, *(unsigned int*)($rx_stream + 4), *(unsigned int*)($rx_stream + 8), *(unsigned int*)($rx_stream + 12), *(unsigned int*)($rx_stream + 20), *(unsigned int*)$rx_mux
printf "TX CR=0x%08x NDTR=%u PAR=0x%08x M0AR=0x%08x FCR=0x%08x DMAMUX=0x%08x\n", *(unsigned int*)$tx_stream, *(unsigned int*)($tx_stream + 4), *(unsigned int*)($tx_stream + 8), *(unsigned int*)($tx_stream + 12), *(unsigned int*)($tx_stream + 20), *(unsigned int*)$tx_mux

delete breakpoints
monitor resume
shell sleep 2
monitor halt

set $magic=(unsigned int)g_das_dma_test_evidence.magic
set $booted=(unsigned int)g_das_dma_test_evidence.booted
set $error=(unsigned int)g_das_dma_test_evidence.error
set $heartbeat=(unsigned int)g_das_dma_test_evidence.heartbeat
set $clock=(int)g_das_dma_test_evidence.clock_result
set $core_clock=(int)g_das_dma_test_evidence.core_clock_result
set $time=(int)g_das_dma_test_evidence.time_result
set $core_hz=(unsigned int)g_das_dma_test_evidence.core_hz
set $flags=(unsigned int)g_das_dma_test_evidence.flags
set $cache_available=(unsigned int)g_das_dma_test_evidence.cache_available
set $cache_enabled=(unsigned int)g_das_dma_test_evidence.cache_enabled
set $cache_line=(unsigned int)g_das_dma_test_evidence.cache_line_size
set $dma_irq=(unsigned int)g_das_dma_test_evidence.dma_irq
set $m2m=(unsigned int)g_das_dma_test_evidence.m2m_bytes
set $spi=(unsigned int)g_das_dma_test_evidence.spi_bytes
set $spi_hz=(unsigned int)g_das_dma_test_evidence.spi_hz
set $mismatch_index=(unsigned int)g_das_dma_test_evidence.mismatch_index
set $expected=(unsigned int)g_das_dma_test_evidence.mismatch_expected
set $actual=(unsigned int)g_das_dma_test_evidence.mismatch_actual

printf "magic=0x%08x booted=%u error=0x%08x heartbeat=%u core_hz=%u flags=0x%02x\n", $magic, $booted, $error, $heartbeat, $core_hz, $flags
printf "cache available=%u enabled=%u line=%u dma_irq=%u\n", $cache_available, $cache_enabled, $cache_line, $dma_irq
printf "dma m2m_bytes=%u spi_bytes=%u spi_hz=%u\n", $m2m, $spi, $spi_hz
printf "mismatch_index=%u expected=0x%02x actual=0x%02x\n", $mismatch_index, $expected, $actual

set $cache_line_expected=0
if $das_expected_cache == 1
  set $cache_line_expected=32
end

if $magic == 0x444d4139 && $booted == 1 && $error == 0 && $heartbeat > 0 && $clock == 0 && $core_clock == 0 && $time == 0 && $core_hz == $das_expected_core_hz && $flags == 0x3f && $cache_available == $das_expected_cache && $cache_enabled == $das_expected_cache && $cache_line == $cache_line_expected && $m2m == 256 && $spi == 192 && $spi_hz == 4000000 && $expected == 0 && $actual == 0
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end

monitor reset halt
detach
quit
