set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

target extended-remote :3333
monitor arm semihosting disable
monitor reset halt

set $cpuid=*(unsigned int*)0xE000ED00
set $part=($cpuid >> 4) & 0xfff
printf "CPUID=0x%08x Cortex-M part=0x%03x\n", $cpuid, $part

if $part == 0xc27
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end

detach
quit
