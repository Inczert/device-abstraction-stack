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
set $pc13=(unsigned int)g_das_button_test_evidence.pc13_level
set $pa0=(unsigned int)g_das_button_test_evidence.pa0_level
set $gpioc_moder=(unsigned int)g_das_button_test_evidence.gpio_c_moder
set $gpioc_pupdr=(unsigned int)g_das_button_test_evidence.gpio_c_pupdr
set $gpioc_idr=(unsigned int)g_das_button_test_evidence.gpio_c_idr
set $gpioa_idr=(unsigned int)g_das_button_test_evidence.gpio_a_idr
set $ahb4enr=(unsigned int)g_das_button_test_evidence.rcc_ahb4enr

printf "expected_pressed=%u error=0x%08x pressed=%u irq=%u press=%u release=%u heartbeat=%u\n", $das_expected_pressed, $error, $pressed, $irq, $press, $release, $heartbeat
printf "button_diag pc13=%u pa0=%u GPIOC_MODER=0x%08x GPIOC_PUPDR=0x%08x GPIOC_IDR=0x%08x GPIOA_IDR=0x%08x AHB4ENR=0x%08x\n", $pc13, $pa0, $gpioc_moder, $gpioc_pupdr, $gpioc_idr, $gpioa_idr, $ahb4enr

set $ok=0
if $das_expected_pressed == 1
  if $error == 0 && $pressed == 1 && $pc13 == 1 && $irq >= 1 && $press >= 1
    set $ok=1
  end
else
  if $error == 0 && $pressed == 0 && $pc13 == 0 && $irq >= 2 && $press >= 1 && $release >= 1
    set $ok=1
  end
end

if $ok == 1
  printf "RESULT: PASS\n"
else
  if $das_expected_pressed == 1 && $pc13 == 0 && $pa0 == 1
    printf "DIAG: B1 activity is visible on PA0, not stock PC13; inspect SB81/SB82 board routing.\n"
  end
  if $das_expected_pressed == 1 && $pc13 == 0 && $pa0 == 0
    printf "DIAG: no asserted B1 level observed on either documented route (PC13 or PA0).\n"
  end
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
