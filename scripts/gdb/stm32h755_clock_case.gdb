set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

monitor arm semihosting disable
monitor reset halt

printf "Flashing STM32H755 board clock-profile qualification image...\n"
load
compare-sections
monitor resume
shell sleep 1
monitor halt

set $magic=(unsigned int)g_das_clock_test_evidence.magic
set $booted=(unsigned int)g_das_clock_test_evidence.booted
set $apply=(int)g_das_clock_test_evidence.apply_result
set $query=(int)g_das_clock_test_evidence.query_result
set $unsupported=(int)g_das_clock_test_evidence.unsupported_result
set $failed=(unsigned int)g_das_clock_test_evidence.failed_frequency_hz
set $profiles=(unsigned int)g_das_clock_test_evidence.profile_mask
set $supported=(unsigned int)g_das_clock_test_evidence.supported_count
set $heartbeat=(unsigned int)g_das_clock_test_evidence.heartbeat
set $sys=(unsigned int)g_das_clock_test_evidence.system_hz
set $cm7=(unsigned int)g_das_clock_test_evidence.cm7_hz
set $cm4=(unsigned int)g_das_clock_test_evidence.cm4_hz
set $ahb=(unsigned int)g_das_clock_test_evidence.ahb_hz
set $apb1=(unsigned int)g_das_clock_test_evidence.apb1_hz
set $apb2=(unsigned int)g_das_clock_test_evidence.apb2_hz
set $apb3=(unsigned int)g_das_clock_test_evidence.apb3_hz
set $apb4=(unsigned int)g_das_clock_test_evidence.apb4_hz
set $pwr_cr3=(unsigned int)g_das_clock_test_evidence.pwr_cr3
set $pwr_csr1=(unsigned int)g_das_clock_test_evidence.pwr_csr1
set $pwr_d3cr=(unsigned int)g_das_clock_test_evidence.pwr_d3cr
set $power_ready=(unsigned int)g_das_clock_test_evidence.power_ready

printf "magic=0x%08x booted=%u apply=%d query=%d unsupported480=%d failed_hz=%u profiles=0x%02x supported=%u heartbeat=%u\n", $magic, $booted, $apply, $query, $unsupported, $failed, $profiles, $supported, $heartbeat
printf "SYS=%u CM7=%u CM4=%u AHB=%u APB1=%u APB2=%u APB3=%u APB4=%u\n", $sys, $cm7, $cm4, $ahb, $apb1, $apb2, $apb3, $apb4
printf "PWR_CR3=0x%08x PWR_CSR1=0x%08x PWR_D3CR=0x%08x power_ready=%u\n", $pwr_cr3, $pwr_csr1, $pwr_d3cr, $power_ready

if $magic == 0x44415343 && $booted == 1 && $apply == 0 && $query == 0 && $unsupported == -2 && $failed == 0 && $profiles == 0x0f && $supported == 4 && $heartbeat > 0 && $sys == 400000000 && $cm7 == 400000000 && $cm4 == 200000000 && $ahb == 200000000 && $apb1 == 100000000 && $apb2 == 100000000 && $apb3 == 100000000 && $apb4 == 100000000 && $power_ready == 1
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end

monitor resume
detach
quit
