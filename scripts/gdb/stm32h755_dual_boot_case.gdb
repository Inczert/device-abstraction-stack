set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off
monitor arm semihosting disable
monitor reset halt
printf "Flashing CM7 boot orchestrator; CM4 bank2 image preprogrammed...\n"
load
compare-sections
# System reset RUN lets hardware autonomously boot either core, according
# to BOOT_CM4 option bytes and the CM7 RCC software request.
monitor reset run
shell sleep 2
monitor halt
set $magic=(unsigned int)g_das_boot_evidence.magic
set $complete=(unsigned int)g_das_boot_evidence.complete
set $error=(unsigned int)g_das_boot_evidence.error
set $request=(int)g_das_boot_evidence.boot_request
set $gate=(unsigned int)g_das_boot_evidence.boot_gate
set $boot_option=(unsigned int)g_das_boot_evidence.boot_option_cm4
set $prequiet=(unsigned int)g_das_boot_evidence.pre_request_quiet
set $preseen=(unsigned int)g_das_boot_evidence.pre_request_seen
set $postseen=(unsigned int)g_das_boot_evidence.post_request_seen
set $initial_req=(unsigned int)g_das_boot_evidence.initial_request
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
printf "CM4 boot gate: required_option=%d measured_option=%d initial_request=%u pre_seen=%u pre_quiet=%u post_seen=%u\n", $das_required_boot_option, $boot_option, $initial_req, $preseen, $prequiet, $postseen
if $magic == 0x44424f54 && $complete == 1 && $error == 0 && $request == 0 && $gate == 1 && ($boot_option == 0 || $boot_option == 1) && ($das_required_boot_option == -1 || $boot_option == $das_required_boot_option) && ($boot_option != 0 || ($initial_req == 0 && $preseen == 0 && $prequiet == 1)) && $postseen == 1 && $domain == 1 && $notify == 1 && $heartbeat > 0 && $boxmagic == 0x44424f54 && $challenge == 0x52434d34 && $response == 0x434d344f && $cm4done == 1 && $cm4hb > 0
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end
monitor reset halt
detach
quit
