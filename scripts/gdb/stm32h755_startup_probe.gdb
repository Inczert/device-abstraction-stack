set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

monitor arm semihosting disable
monitor halt

# Dirty one .data object and one .bss object in RAM. A real reset through the
# reusable Cortex-M Reset_Handler must restore/clear both before main() runs.
set var g_das_startup_data_probe = 0xdeadbeef
set var g_das_startup_bss_probe = 0xa5a5a5a5

monitor reset halt
monitor resume
shell sleep 1
monitor halt

set $data_probe=(unsigned int)g_das_startup_data_probe
set $bss_probe=(unsigned int)g_das_startup_bss_probe
set $vtor=*(unsigned int*)0xe000ed08
set $vector=(unsigned int)&g_das_vector_table
set $booted=(unsigned int)g_das_hw_evidence.booted
set $error=(unsigned int)g_das_hw_evidence.error
set $heartbeat=(unsigned int)g_das_hw_evidence.heartbeat

printf "startup data=0x%08x bss=0x%08x VTOR=0x%08x vector=0x%08x\n", $data_probe, $bss_probe, $vtor, $vector
printf "booted=%u error=0x%08x heartbeat=%u\n", $booted, $error, $heartbeat

if $data_probe == 0x13579bdf && $bss_probe == 0 && $vtor == $vector && $booted == 1 && $error == 0 && $heartbeat > 0
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end

monitor resume
detach
quit
