set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

monitor arm semihosting disable
monitor reset halt

printf "Flashing STM32H755 timer/PWM qualification image...\n"
load
compare-sections
monitor resume
shell sleep 2
monitor halt

set $magic=(unsigned int)g_das_timer_test_evidence.magic
set $booted=(unsigned int)g_das_timer_test_evidence.booted
set $error=(unsigned int)g_das_timer_test_evidence.error
set $heartbeat=(unsigned int)g_das_timer_test_evidence.heartbeat
set $clock=(int)g_das_timer_test_evidence.clock_result
set $core_clock=(int)g_das_timer_test_evidence.core_clock_result
set $core_hz=(unsigned int)g_das_timer_test_evidence.core_hz
set $flags=(unsigned int)g_das_timer_test_evidence.flags
set $timer_hz=(unsigned int)g_das_timer_test_evidence.timer_hz
set $irq_count=(unsigned int)g_das_timer_test_evidence.timer_irq_count
set $elapsed=(unsigned int)g_das_timer_test_evidence.timer_elapsed_cycles
set $expected=(unsigned int)g_das_timer_test_evidence.timer_expected_cycles
set $levels=(unsigned int)g_das_timer_test_evidence.irq_priority_levels
set $priority=(unsigned int)g_das_timer_test_evidence.irq_priority
set $pwm_hz=(unsigned int)g_das_timer_test_evidence.pwm_hz
set $pwm_period=(unsigned int)g_das_timer_test_evidence.pwm_period_cycles
set $pwm_high=(unsigned int)g_das_timer_test_evidence.pwm_high_cycles
set $pwm_duty=(unsigned int)g_das_timer_test_evidence.pwm_duty_per_mille

printf "magic=0x%08x booted=%u error=0x%08x heartbeat=%u core_hz=%u flags=0x%03x\n", $magic, $booted, $error, $heartbeat, $core_hz, $flags
printf "timer_hz=%u irq_count=%u elapsed_cycles=%u expected_cycles=%u irq_levels=%u irq_priority=%u\n", $timer_hz, $irq_count, $elapsed, $expected, $levels, $priority
printf "pwm_hz=%u final_duty_permille=%u period_cycles=%u high_cycles=%u\n", $pwm_hz, $pwm_duty, $pwm_period, $pwm_high

if $magic == 0x44544d52 && $booted == 1 && $error == 0 && $heartbeat > 0 && $clock == 0 && $core_clock == 0 && $flags == 0x1ff && $timer_hz > 0 && $irq_count >= 101 && $pwm_hz > 0
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end

monitor reset halt
detach
quit
