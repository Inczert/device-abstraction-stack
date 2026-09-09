set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

monitor arm semihosting disable
monitor reset halt

printf "Flashing STM32H755 UART loopback qualification image...\n"
load
compare-sections
monitor resume
shell sleep 2
monitor halt

set $magic=(unsigned int)g_das_uart_test_evidence.magic
set $booted=(unsigned int)g_das_uart_test_evidence.booted
set $error=(unsigned int)g_das_uart_test_evidence.error
set $heartbeat=(unsigned int)g_das_uart_test_evidence.heartbeat
set $clock=(int)g_das_uart_test_evidence.clock_result
set $core_clock=(int)g_das_uart_test_evidence.core_clock_result
set $time=(int)g_das_uart_test_evidence.time_result
set $timeout=(int)g_das_uart_test_evidence.timeout_result
set $core_hz=(unsigned int)g_das_uart_test_evidence.core_hz
set $flags=(unsigned int)g_das_uart_test_evidence.flags
set $baud1=(unsigned int)g_das_uart_test_evidence.baud_115200
set $baud2=(unsigned int)g_das_uart_test_evidence.baud_57600
set $baud3=(unsigned int)g_das_uart_test_evidence.baud_38400
set $bytes=(unsigned int)g_das_uart_test_evidence.bytes_checked
set $expected=(unsigned int)g_das_uart_test_evidence.mismatch_expected
set $actual=(unsigned int)g_das_uart_test_evidence.mismatch_actual

printf "magic=0x%08x booted=%u error=0x%08x heartbeat=%u core_hz=%u\n", $magic, $booted, $error, $heartbeat, $core_hz
printf "clock=%d core_clock=%d time=%d timeout=%d flags=0x%02x bytes=%u\n", $clock, $core_clock, $time, $timeout, $flags, $bytes
printf "baud115200=%u baud57600=%u baud38400=%u mismatch_expected=0x%02x mismatch_actual=0x%02x\n", $baud1, $baud2, $baud3, $expected, $actual

if $magic == 0x44554152 && $booted == 1 && $error == 0 && $heartbeat > 0 && $clock == 0 && $core_clock == 0 && $time == 0 && $timeout == -3 && $flags == 0x1f && $bytes == 34
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end

monitor reset halt
detach
quit
