set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off
monitor arm semihosting disable
monitor reset halt
printf "Flashing software-reset + watchdog-reset qualification image (expected watchdog bit 0x%x)...\n", $das_expected_watchdog_mask
load
compare-sections

# Start fresh, then the firmware itself must reboot TWICE (software, IWDG).
# No test step manually resets the chip after this point.
set variable g_das_watchdog_stage = 0
set variable g_das_watchdog_saved_software = 0
monitor resume
shell sleep 6
monitor halt

set $magic=(unsigned int)g_das_watchdog_evidence.magic
set $complete=(unsigned int)g_das_watchdog_evidence.complete
set $error=(unsigned int)g_das_watchdog_evidence.error
set $start=(int)g_das_watchdog_evidence.start_result
set $actual=(unsigned int)g_das_watchdog_evidence.actual_ms
set $software=(unsigned int)g_das_watchdog_evidence.software_flags
set $flags=(unsigned int)g_das_watchdog_evidence.reset_flags
set $raw=(unsigned int)g_das_watchdog_evidence.raw_rsr
set $cleared=(unsigned int)g_das_watchdog_evidence.cleared_flags
set $stage=(unsigned int)g_das_watchdog_stage
set $heartbeat=(unsigned int)g_das_watchdog_evidence.heartbeat
printf "WD magic=0x%08x complete=%u error=0x%08x start=%d actual=%u ms\n", $magic, $complete, $error, $start, $actual
printf "WD software_flags=0x%08x IWDG_flags=0x%08x RCC_RSR=0x%08x cleared_flags=0x%08x stage=0x%08x heartbeat=%u\n", $software, $flags, $raw, $cleared, $stage, $heartbeat

if $magic == 0x44535744 && $complete == 1 && $error == 0 && $start == 0 && $actual == 1200 && ($software & 0x08) != 0 && ($software & $das_expected_watchdog_mask) == 0 && ($flags & $das_expected_watchdog_mask) != 0 && ($flags & 0x08) == 0 && ($cleared & ($das_expected_watchdog_mask | 0x08)) == 0 && $stage == 0 && $heartbeat > 0
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end
monitor reset halt
detach
quit
