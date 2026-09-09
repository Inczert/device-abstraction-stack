set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

monitor arm semihosting disable
monitor reset halt

printf "Flashing STM32H755 HSI/PLL clock qualification image...\n"
load
compare-sections
monitor resume
shell sleep 1
monitor halt

set $magic=(unsigned int)g_das_clock_test_evidence.magic
set $booted=(unsigned int)g_das_clock_test_evidence.booted
set $apply=(int)g_das_clock_test_evidence.apply_result
set $query=(int)g_das_clock_test_evidence.query_result
set $system=(unsigned int)g_das_clock_test_evidence.system_hz
set $cm7=(unsigned int)g_das_clock_test_evidence.cm7_hz
set $cm4=(unsigned int)g_das_clock_test_evidence.cm4_hz
set $ahb=(unsigned int)g_das_clock_test_evidence.ahb_hz
set $apb1=(unsigned int)g_das_clock_test_evidence.apb1_hz
set $apb2=(unsigned int)g_das_clock_test_evidence.apb2_hz
set $apb3=(unsigned int)g_das_clock_test_evidence.apb3_hz
set $apb4=(unsigned int)g_das_clock_test_evidence.apb4_hz
set $heartbeat=(unsigned int)g_das_clock_test_evidence.heartbeat

printf "magic=0x%08x booted=%u apply=%d query=%d heartbeat=%u\n", $magic, $booted, $apply, $query, $heartbeat
printf "SYS=%u CM7=%u CM4=%u AHB=%u APB1=%u APB2=%u APB3=%u APB4=%u\n", $system, $cm7, $cm4, $ahb, $apb1, $apb2, $apb3, $apb4

if $magic == 0x44415343 && $booted == 1 && $apply == 0 && $query == 0 && $heartbeat > 0 && $system == 400000000 && $cm7 == 400000000 && $cm4 == 200000000 && $ahb == 200000000 && $apb1 == 100000000 && $apb2 == 100000000 && $apb3 == 100000000 && $apb4 == 100000000
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end

# Return the board to reset clock state before the normal CM7 firmware phase.
monitor reset halt
detach
quit
