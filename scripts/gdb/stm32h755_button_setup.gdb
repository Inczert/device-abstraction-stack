set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

monitor arm semihosting disable
monitor reset halt

printf "Flashing STM32H755 board-resource/button qualification image...\n"
load
compare-sections
monitor resume
shell sleep 1
monitor halt

set $magic=(unsigned int)g_das_button_test_evidence.magic
set $booted=(unsigned int)g_das_button_test_evidence.booted
set $error=(unsigned int)g_das_button_test_evidence.error
set $heartbeat=(unsigned int)g_das_button_test_evidence.heartbeat
set $map=(unsigned int)g_das_button_test_evidence.map_flags
set $ready=(unsigned int)g_das_button_test_evidence.ready
set $pressed=(unsigned int)g_das_button_test_evidence.pressed
set $levels=(unsigned int)g_das_button_test_evidence.irq_priority_levels
set $priority=(unsigned int)g_das_button_test_evidence.irq_priority

printf "magic=0x%08x booted=%u error=0x%08x heartbeat=%u map=0x%02x ready=%u pressed=%u irq_levels=%u irq_priority=%u\n", $magic, $booted, $error, $heartbeat, $map, $ready, $pressed, $levels, $priority

if $magic == 0x44415342 && $booted == 1 && $error == 0 && $heartbeat > 0 && $map == 0x1f && $ready == 1 && $pressed == 0 && $levels > 0 && $priority < $levels
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end

monitor resume
detach
quit
