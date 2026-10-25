set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off
monitor arm semihosting disable
monitor reset halt
printf "Flashing watchdog + reset-reason qualification image...\n"
load
compare-sections

# The one-time .noinit sentinel is set BEFORE the firmware begins and is
# preserved by the hardware IWDG-triggered system reset.
set variable g_das_watchdog_stage = 0
monitor resume
shell sleep 5
monitor halt

set $magic=(unsigned int)g_das_watchdog_evidence.magic
set $complete=(unsigned int)g_das_watchdog_evidence.complete
set $error=(unsigned int)g_das_watchdog_evidence.error
set $start=(int)g_das_watchdog_evidence.start_result
set $actual=(unsigned int)g_das_watchdog_evidence.actual_ms
set $flags=(unsigned int)g_das_watchdog_evidence.reset_flags
set $raw=(unsigned int)g_das_watchdog_evidence.raw_rsr
set $cleared=(unsigned int)g_das_watchdog_evidence.cleared_flags
set $stage=(unsigned int)g_das_watchdog_stage
set $heartbeat=(unsigned int)g_das_watchdog_evidence.heartbeat
printf "WD magic=0x%08x complete=%u error=0x%08x start=%d actual=%u ms\n", $magic, $complete, $error, $start, $actual
printf "WD flags=0x%08x RCC_RSR=0x%08x cleared_flags=0x%08x stage=0x%08x heartbeat=%u\n", $flags, $raw, $cleared, $stage, $heartbeat

if $magic == 0x44535744 && $complete == 1 && $error == 0 && $start == 0 && $actual == 1200 && ($flags & 0x10) != 0 && ($cleared & 0x10) == 0 && $stage == 0 && $heartbeat > 0
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end
# Leave the target at reset for the next independent test firmware.
monitor reset halt
detach
quit
