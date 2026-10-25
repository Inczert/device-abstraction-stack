set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off
monitor arm semihosting disable
monitor reset halt
load
compare-sections
monitor resume
shell sleep 1
monitor halt

set $magic=(unsigned int)g_das_hsem_evidence.magic
set $complete=(unsigned int)g_das_hsem_evidence.complete
set $error=(unsigned int)g_das_hsem_evidence.error
set $flags=(unsigned int)g_das_hsem_evidence.flags
set $request=(int)g_das_hsem_evidence.request_result
set $heartbeat=(unsigned int)g_das_hsem_evidence.heartbeat
printf "magic=0x%08x complete=%u error=0x%08x flags=0x%02x request=%d heartbeat=%u\n", $magic, $complete, $error, $flags, $request, $heartbeat
if $magic == 0x44534853 && $complete == 1 && $error == 0 && $flags == 0x0f && $request == $das_expected_request && $heartbeat > 0
  printf "RESULT: PASS\n"
else
  printf "RESULT: FAIL\n"
  quit 1
end
monitor reset halt
detach
quit
