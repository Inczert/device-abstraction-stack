set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

monitor arm semihosting disable
monitor reset halt

printf "Flashing STM32H755 monotonic-time qualification image...\n"
load
compare-sections
monitor resume
shell sleep 1
monitor halt

set $magic=(unsigned int)g_das_time_test_evidence.magic
set $booted=(unsigned int)g_das_time_test_evidence.booted
set $error=(unsigned int)g_das_time_test_evidence.error
set $not_ready=(int)g_das_time_test_evidence.not_ready_result
set $source=(int)g_das_time_test_evidence.source_result
set $clock=(int)g_das_time_test_evidence.clock_result
set $core_query=(int)g_das_time_test_evidence.core_query_result
set $init=(int)g_das_time_test_evidence.init_result
set $delay=(int)g_das_time_test_evidence.delay_result
set $invalid_deadline=(int)g_das_time_test_evidence.invalid_deadline_result
set $wrap_flags=(unsigned int)g_das_time_test_evidence.wrap_flags
set $wrap_elapsed=(unsigned int)g_das_time_test_evidence.wrap_elapsed_ms
set $wrap_deadline=(unsigned int)g_das_time_test_evidence.wrap_deadline_ms
set $core_hz=(unsigned int)g_das_time_test_evidence.core_hz
set $start=(unsigned int)g_das_time_test_evidence.start_ms
set $end=(unsigned int)g_das_time_test_evidence.end_ms
set $elapsed=(unsigned int)g_das_time_test_evidence.elapsed_ms
set $measured=(unsigned int)g_das_time_test_evidence.measured_cycles
set $expected=(unsigned int)g_das_time_test_evidence.expected_cycles
set $tolerance=(unsigned int)g_das_time_test_evidence.tolerance_cycles
set $timing_ok=(unsigned int)g_das_time_test_evidence.timing_ok
set $ready=(unsigned int)g_das_time_test_evidence.source_ready
set $heartbeat=(unsigned int)g_das_time_test_evidence.heartbeat

printf "magic=0x%08x booted=%u error=0x%08x heartbeat=%u\n", $magic, $booted, $error, $heartbeat
printf "not_ready=%d source=%d clock=%d core_query=%d init=%d delay=%d invalid_deadline=%d ready=%u\n", $not_ready, $source, $clock, $core_query, $init, $delay, $invalid_deadline, $ready
printf "wrap_flags=0x%02x wrap_elapsed=%u wrap_deadline=%u\n", $wrap_flags, $wrap_elapsed, $wrap_deadline
printf "core_hz=%u start=%u end=%u elapsed=%u cycles=%u expected=%u tolerance=%u timing_ok=%u\n", $core_hz, $start, $end, $elapsed, $measured, $expected, $tolerance, $timing_ok

if $magic == 0x44415354 && $booted == 1 && $error == 0 && $not_ready == -4 && $source == 0 && $clock == 0 && $core_query == 0 && $init == 0 && $delay == 0 && $invalid_deadline == -1 && $wrap_flags == 0x1f && $wrap_elapsed == 9 && $wrap_deadline == 2 && $core_hz > 0 && $elapsed >= 100 && $elapsed <= 102 && $measured > 0 && $expected > 0 && $timing_ok == 1 && $ready == 1 && $heartbeat > 0
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end

# Restore reset clock/state before the next independent campaign image.
monitor reset halt
detach
quit
