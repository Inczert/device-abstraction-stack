set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

monitor arm semihosting disable
monitor reset halt

printf "Flashing STM32H755 SPI qualification image...\n"
load
compare-sections
monitor resume
shell sleep 2
monitor halt

set $magic=(unsigned int)g_das_spi_test_evidence.magic
set $booted=(unsigned int)g_das_spi_test_evidence.booted
set $error=(unsigned int)g_das_spi_test_evidence.error
set $heartbeat=(unsigned int)g_das_spi_test_evidence.heartbeat
set $clock=(int)g_das_spi_test_evidence.clock_result
set $core_clock=(int)g_das_spi_test_evidence.core_clock_result
set $time=(int)g_das_spi_test_evidence.time_result
set $core_hz=(unsigned int)g_das_spi_test_evidence.core_hz
set $flags=(unsigned int)g_das_spi_test_evidence.flags
set $hz0=(unsigned int)g_das_spi_test_evidence.hz_mode0
set $hz1=(unsigned int)g_das_spi_test_evidence.hz_mode1
set $hz2=(unsigned int)g_das_spi_test_evidence.hz_mode2
set $hz3=(unsigned int)g_das_spi_test_evidence.hz_mode3
set $bytes=(unsigned int)g_das_spi_test_evidence.bytes_checked
set $cs_selected=(unsigned int)g_das_spi_test_evidence.cs_selected
set $cs_inactive=(unsigned int)g_das_spi_test_evidence.cs_inactive
set $expected=(unsigned int)g_das_spi_test_evidence.mismatch_expected
set $actual=(unsigned int)g_das_spi_test_evidence.mismatch_actual

printf "magic=0x%08x booted=%u error=0x%08x heartbeat=%u core_hz=%u flags=0x%03x\n", $magic, $booted, $error, $heartbeat, $core_hz, $flags
printf "spi_hz mode0=%u mode1=%u mode2=%u mode3=%u bytes=%u cs_selected=%u cs_inactive=%u\n", $hz0, $hz1, $hz2, $hz3, $bytes, $cs_selected, $cs_inactive
printf "mismatch_expected=0x%02x mismatch_actual=0x%02x\n", $expected, $actual

if $magic == 0x44535049 && $booted == 1 && $error == 0 && $heartbeat > 0 && $clock == 0 && $core_clock == 0 && $time == 0 && $flags == 0x1ff && $hz0 == 1000000 && $hz1 == 2000000 && $hz2 == 4000000 && $hz3 == 8000000 && $bytes == 111 && $cs_selected == 1 && $cs_inactive == 1 && $expected == 0 && $actual == 0
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end

monitor reset halt
detach
quit
