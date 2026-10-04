set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

monitor arm semihosting disable
monitor reset halt

printf "Flashing DAS STM32H755 hard-float startup image...\n"
load
compare-sections
monitor resume
shell sleep 1
monitor halt

set $started=(unsigned int)g_das_fpu_started
set $captured_cpacr=(unsigned int)g_das_fpu_cpacr
set $live_cpacr=*(unsigned int*)0xe000ed88
set $captured_cfsr=(unsigned int)g_das_fpu_cfsr
set $live_cfsr=*(unsigned int*)0xe000ed28
set $result_bits=(unsigned int)g_das_fpu_result_bits
set $fault=(unsigned int)g_das_fpu_fault
set $pass=(unsigned int)g_das_fpu_pass
set $heartbeat=(unsigned int)g_das_fpu_heartbeat

printf "FPU started=%u pass=%u fault=%u heartbeat=%u\n", $started, $pass, $fault, $heartbeat
printf "FPU CPACR captured=0x%08x live=0x%08x CFSR captured=0x%08x live=0x%08x result_bits=0x%08x\n", $captured_cpacr, $live_cpacr, $captured_cfsr, $live_cfsr, $result_bits

if $started == 1 && $pass == 1 && $fault == 0 && $heartbeat > 0 && ($captured_cpacr & 0x00f00000) == 0x00f00000 && ($live_cpacr & 0x00f00000) == 0x00f00000 && ($captured_cfsr & 0x00080000) == 0 && ($live_cfsr & 0x00080000) == 0 && $result_bits == 0x40780000
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end

monitor resume
detach
quit
