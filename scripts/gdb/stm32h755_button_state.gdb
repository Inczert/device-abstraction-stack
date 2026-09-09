set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

monitor halt

set $error=(unsigned int)g_das_button_test_evidence.error
set $pressed=(unsigned int)g_das_button_test_evidence.pressed
set $irq=(unsigned int)g_das_button_test_evidence.irq_count
set $press=(unsigned int)g_das_button_test_evidence.press_count
set $release=(unsigned int)g_das_button_test_evidence.release_count
set $heartbeat=(unsigned int)g_das_button_test_evidence.heartbeat

printf "expected_pressed=%u error=0x%08x pressed=%u irq=%u press=%u release=%u heartbeat=%u\n", $das_expected_pressed, $error, $pressed, $irq, $press, $release, $heartbeat

set $ok=0
if $das_expected_pressed == 1
  if $error == 0 && $pressed == 1 && $irq >= 1 && $press >= 1
    set $ok=1
  end
else
  if $error == 0 && $pressed == 0 && $irq >= 2 && $press >= 1 && $release >= 1
    set $ok=1
  end
end

if $ok == 1
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end

if $das_expected_pressed == 1
  monitor resume
else
  monitor reset halt
end

detach
quit
