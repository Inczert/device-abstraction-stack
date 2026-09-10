set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

monitor arm semihosting disable
monitor reset halt

printf "Flashing STM32H755 I2C qualification image...\n"
load
compare-sections
monitor resume
shell sleep 3
monitor halt

set $magic=(unsigned int)g_das_i2c_test_evidence.magic
set $booted=(unsigned int)g_das_i2c_test_evidence.booted
set $error=(unsigned int)g_das_i2c_test_evidence.error
set $heartbeat=(unsigned int)g_das_i2c_test_evidence.heartbeat
set $clock=(int)g_das_i2c_test_evidence.clock_result
set $core_clock=(int)g_das_i2c_test_evidence.core_clock_result
set $time=(int)g_das_i2c_test_evidence.time_result
set $core_hz=(unsigned int)g_das_i2c_test_evidence.core_hz
set $flags=(unsigned int)g_das_i2c_test_evidence.flags
set $standard_hz=(unsigned int)g_das_i2c_test_evidence.standard_hz
set $fast_hz=(unsigned int)g_das_i2c_test_evidence.fast_hz
set $missing=(int)g_das_i2c_test_evidence.missing_probe_result
set $bytes=(unsigned int)g_das_i2c_test_evidence.bytes_checked
set $addresses=(unsigned int)g_das_i2c_test_evidence.target_address_events
set $stops=(unsigned int)g_das_i2c_test_evidence.target_stop_events
set $target_errors=(unsigned int)g_das_i2c_test_evidence.target_error_events
set $mismatch_index=(unsigned int)g_das_i2c_test_evidence.mismatch_index
set $expected=(unsigned int)g_das_i2c_test_evidence.mismatch_expected
set $actual=(unsigned int)g_das_i2c_test_evidence.mismatch_actual

printf "magic=0x%08x booted=%u error=0x%08x heartbeat=%u core_hz=%u flags=0x%03x\n", $magic, $booted, $error, $heartbeat, $core_hz, $flags
printf "i2c_hz standard=%u fast=%u missing_probe=%d bytes=%u target_addr=%u target_stop=%u target_errors=%u\n", $standard_hz, $fast_hz, $missing, $bytes, $addresses, $stops, $target_errors
printf "mismatch_index=%u expected=0x%02x actual=0x%02x\n", $mismatch_index, $expected, $actual

set $standard_ok=($standard_hz >= 95000 && $standard_hz <= 105000)
set $fast_ok=($fast_hz >= 380000 && $fast_hz <= 420000)

if $magic == 0x44493243 && $booted == 1 && $error == 0 && $heartbeat > 0 && $clock == 0 && $core_clock == 0 && $time == 0 && $flags == 0x1ff && $standard_ok && $fast_ok && $missing == -5 && $bytes == 70 && $addresses >= 7 && $stops >= 5 && $target_errors == 0
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end

monitor reset halt
detach
quit
