set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off
monitor arm semihosting disable
monitor reset halt
printf "Flashing CM7 boot orchestrator; CM4 bank2 image preprogrammed...\n"
load
compare-sections
monitor reset halt
monitor resume
shell sleep 2
monitor halt
set $magic=(unsigned int)g_das_boot_evidence.magic
set $complete=(unsigned int)g_das_boot_evidence.complete
set $error=(unsigned int)g_das_boot_evidence.error
set $request=(int)g_das_boot_evidence.boot_request
set $gate=(unsigned int)g_das_boot_evidence.boot_gate
set $boot_option=(unsigned int)g_das_boot_evidence.boot_option_cm4
set $domain=(unsigned int)g_das_boot_evidence.domain_ready
set $notify=(unsigned int)g_das_boot_evidence.notification
set $heartbeat=(unsigned int)g_das_boot_evidence.heartbeat
set $boxmagic=*(unsigned int*)0x30040000
set $challenge=*(unsigned int*)0x30040004
set $response=*(unsigned int*)0x30040008
set $cm4done=*(unsigned int*)0x3004000c
set $cm4hb=*(unsigned int*)0x30040010
printf "CM7: complete=%u error=%u request=%d gate=%u boot_option_cm4=%u d2ready=%u notify=%u heartbeat=%u\n", $complete, $error, $request, $gate, $boot_option, $domain, $notify, $heartbeat
printf "CM4: magic=0x%08x challenge=0x%08x response=0x%08x done=%u hb=%u\n", $boxmagic, $challenge, $response, $cm4done, $cm4hb
if $magic == 0x44424f54 && $complete == 1 && $error == 0 && $request == 0 && $gate == 1 && ($boot_option == 0 || $boot_option == 1) && $domain == 1 && $notify == 1 && $heartbeat > 0 && $boxmagic == 0x44424f54 && $challenge == 0x52434d34 && $response == 0x434d344f && $cm4done == 1 && $cm4hb > 0
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end
monitor reset halt
detach
quit
