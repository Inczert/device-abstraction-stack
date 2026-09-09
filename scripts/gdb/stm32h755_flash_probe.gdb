set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

target extended-remote :3333
monitor arm semihosting disable
monitor reset halt

printf "Flashing DAS STM32H755 hardware-test image...\n"
load
compare-sections
monitor reset run
shell sleep 1
monitor halt

set $magic=(unsigned int)g_das_hw_evidence.magic
set $booted=(unsigned int)g_das_hw_evidence.booted
set $error=(unsigned int)g_das_hw_evidence.error
set $heartbeat=(unsigned int)g_das_hw_evidence.heartbeat
set $rcc=(unsigned int)g_das_hw_evidence.rcc_ahb4enr
set $pb_mode=(unsigned int)g_das_hw_evidence.gpio_b_moder
set $pe_mode=(unsigned int)g_das_hw_evidence.gpio_e_moder

printf "magic=0x%08x booted=%u error=0x%08x heartbeat=%u\n", $magic, $booted, $error, $heartbeat
printf "AHB4ENR=0x%08x GPIOB_MODER=0x%08x GPIOE_MODER=0x%08x\n", $rcc, $pb_mode, $pe_mode

set $pb0_mode=($pb_mode >> 0) & 3
set $pb14_mode=($pb_mode >> 28) & 3
set $pe1_mode=($pe_mode >> 2) & 3
set $clocks=$rcc & ((1 << 1) | (1 << 4))

if $magic == 0x44415331 && $booted == 1 && $error == 0 && $heartbeat > 0 && $pb0_mode == 1 && $pb14_mode == 1 && $pe1_mode == 1 && $clocks == ((1 << 1) | (1 << 4))
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end

monitor resume
detach
quit
