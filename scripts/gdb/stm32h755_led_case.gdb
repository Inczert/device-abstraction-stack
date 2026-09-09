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
set $mask=(unsigned int)g_das_hw_evidence.led_mask
set $blinks=(unsigned int)g_das_hw_evidence.blink_count
set $pb_odr=(unsigned int)g_das_hw_evidence.gpio_b_odr
set $pe_odr=(unsigned int)g_das_hw_evidence.gpio_e_odr
set $register_mask=(($pb_odr >> 0) & 1) | ((($pe_odr >> 1) & 1) << 1) | ((($pb_odr >> 14) & 1) << 2)

printf "command=%u seen=%u error=0x%08x led_mask=0x%x register_mask=0x%x blinks=%u\n", $das_command, $seen, $error, $mask, $register_mask, $blinks

if $das_command == 5
  if $error == 0 && $seen == $das_command && $blinks > 0
    printf "RESULT: PASS\n"
  else
    printf "RESULT: FAIL\n"
    quit 1
  end
else
  if $error == 0 && $seen == $das_command && $mask == $das_expected_mask && $register_mask == $das_expected_mask
    printf "RESULT: PASS\n"
  else
    printf "RESULT: FAIL\n"
    quit 1
  end
end

monitor resume
detach
quit
