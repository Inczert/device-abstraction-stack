set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

target extended-remote :3333
monitor halt
set variable g_das_hw_command = $das_command
monitor resume
shell sleep 1
monitor halt

set $error=(unsigned int)g_das_hw_evidence.error
set $seen=(unsigned int)g_das_hw_evidence.command_seen
set $flags=(unsigned int)g_das_hw_evidence.gpio_test_flags
set $low=(unsigned int)g_das_hw_evidence.gpio_input_low
set $high=(unsigned int)g_das_hw_evidence.gpio_input_high
set $irq=(unsigned int)g_das_hw_evidence.exti_irq_count
set $rising=(unsigned int)g_das_hw_evidence.exti_rising_count
set $falling=(unsigned int)g_das_hw_evidence.exti_falling_count

printf "command=%u seen=%u error=0x%08x flags=0x%08x low=%u high=%u irq=%u rising=%u falling=%u\n", $das_command, $seen, $error, $flags, $low, $high, $irq, $rising, $falling

if $error == 0 && $seen == $das_command && ($flags & $das_expected_flags) == $das_expected_flags
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end

monitor resume
detach
quit
