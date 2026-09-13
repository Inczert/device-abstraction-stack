set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

monitor halt

set $link_up=(unsigned int)g_das_eth_link_up
set $speed=(unsigned int)g_das_eth_speed_mbps
set $duplex=(unsigned int)g_das_eth_duplex
set $last_result=(int)g_das_eth_last_result

printf "ETH_LINK_CHECK expected_up=%u actual_up=%u speed_mbps=%u duplex=%u last_result=%d\n", $das_expected_link_up, $link_up, $speed, $duplex, $last_result

if $das_expected_link_up == 0
  if $link_up == 0 && $speed == 0 && $duplex == 0 && $last_result == 0
    printf "RESULT: PASS\n"
    monitor resume
    detach
    quit
  else
    printf "RESULT: FAIL\n"
    monitor resume
    detach
    quit 1
  end
else
  set $speed_valid=($speed == 10 || $speed == 100)
  set $duplex_valid=($duplex == 1 || $duplex == 2)
  if $link_up == 1 && $speed_valid && $duplex_valid && $last_result == 0
    printf "RESULT: PASS\n"
    monitor resume
    detach
    quit
  else
    printf "RESULT: FAIL\n"
    monitor resume
    detach
    quit 1
  end
end
