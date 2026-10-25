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
set variable g_das_watchdog_boot_count = 0
set variable g_das_watchdog_option_status = 0
set variable g_das_watchdog_hw_start = 0
set variable g_das_wd_dbg_before = 0
set variable g_das_wd_dbg_after = 0
set variable g_das_wd_clock_csr = 0
set variable g_das_wd_pr = 0
set variable g_das_wd_rlr = 0
set variable g_das_wd_sr = 0
set variable g_das_wd_start_return = -4
set variable g_das_wd_start_actual = 0
set variable g_das_wd_checkpoint = 0
# Run from a fresh whole-device reset with no debugger-assisted restart
# between the two deliberate firmware-triggered resets.
monitor reset run
shell sleep 6
monitor halt

set $magic=(unsigned int)g_das_watchdog_evidence.magic
set $complete=(unsigned int)g_das_watchdog_evidence.complete
set $error=(unsigned int)g_das_watchdog_evidence.error
set $start=(int)g_das_watchdog_evidence.start_result
set $actual=(unsigned int)g_das_watchdog_evidence.actual_ms
set $software=(unsigned int)g_das_watchdog_evidence.software_flags
set $persisted_software=(unsigned int)g_das_watchdog_saved_software
set $boots=(unsigned int)g_das_watchdog_boot_count
set $option=(unsigned int)g_das_watchdog_option_status
set $hardware_started=(unsigned int)g_das_watchdog_hw_start
set $dbg_before=(unsigned int)g_das_wd_dbg_before
set $dbg_after=(unsigned int)g_das_wd_dbg_after
set $lsi_csr=(unsigned int)g_das_wd_clock_csr
set $iwdg_pr=(unsigned int)g_das_wd_pr
set $iwdg_rlr=(unsigned int)g_das_wd_rlr
set $iwdg_sr=(unsigned int)g_das_wd_sr
set $config_return=(int)g_das_wd_start_return
set $config_actual=(unsigned int)g_das_wd_start_actual
set $checkpoint=(unsigned int)g_das_wd_checkpoint
set $flags=(unsigned int)g_das_watchdog_evidence.reset_flags
set $raw=(unsigned int)g_das_watchdog_evidence.raw_rsr
set $cleared=(unsigned int)g_das_watchdog_evidence.cleared_flags
set $stage=(unsigned int)g_das_watchdog_stage
set $heartbeat=(unsigned int)g_das_watchdog_evidence.heartbeat
printf "WD magic=0x%08x complete=%u error=0x%08x start=%d actual=%u ms\n", $magic, $complete, $error, $start, $actual
printf "WD software_flags=0x%08x saved_software=0x%08x IWDG_flags=0x%08x RCC_RSR=0x%08x cleared_flags=0x%08x\n", $software, $persisted_software, $flags, $raw, $cleared
printf "WD boots=%u stage=0x%08x FLASH_OPTSR_CUR=0x%08x hardware_IWDG=%u heartbeat=%u\n", $boots, $stage, $option, $hardware_started, $heartbeat
printf "WD DBG freeze before=0x%08x after=0x%08x LSI_RCC_CSR=0x%08x PR=0x%08x RLR=0x%08x SR=0x%08x\n", $dbg_before, $dbg_after, $lsi_csr, $iwdg_pr, $iwdg_rlr, $iwdg_sr
printf "WD start_return=%d nominal_ms=%u checkpoint=%u (0=in start,1=returned,2=fed/armed,3=snapshotted)\n", $config_return, $config_actual, $checkpoint

if $magic == 0x44535744 && $complete == 1 && $error == 0 && $start == 0 && $actual == 1200 && ($software & 0x08) != 0 && ($software & $das_expected_watchdog_mask) == 0 && ($flags & $das_expected_watchdog_mask) != 0 && ($flags & 0x08) == 0 && ($cleared & ($das_expected_watchdog_mask | 0x08)) == 0 && $config_return == 0 && $config_actual == 1200 && $checkpoint >= 2 && $stage == 0 && $boots == 3 && $heartbeat > 0
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end
monitor reset halt
detach
quit
