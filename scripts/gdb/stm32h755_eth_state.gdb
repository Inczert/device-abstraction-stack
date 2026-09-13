set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

monitor halt

set $link_up=(unsigned int)g_das_eth_link_up
set $speed=(unsigned int)g_das_eth_speed_mbps
set $duplex=(unsigned int)g_das_eth_duplex
set $tx_count=(unsigned int)g_das_eth_tx_count
set $rx_count=(unsigned int)g_das_eth_rx_count
set $rx_bytes=(unsigned int)g_das_eth_rx_bytes
set $rx_test_count=(unsigned int)g_das_eth_rx_test_count
set $rx_test_errors=(unsigned int)g_das_eth_rx_test_errors
set $rx_last_sequence=(unsigned int)g_das_eth_rx_last_sequence
set $last_result=(int)g_das_eth_last_result

printf "ETH_LINK up=%u speed_mbps=%u duplex=%u\n", $link_up, $speed, $duplex
printf "ETH_TX count=%u\n", $tx_count
printf "ETH_RX count=%u bytes=%u test_count=%u test_errors=%u last_sequence=%u\n", $rx_count, $rx_bytes, $rx_test_count, $rx_test_errors, $rx_last_sequence
printf "ETH_RESULT last_result=%d\n", $last_result

set $speed_valid=($speed == 10 || $speed == 100)
set $duplex_valid=($duplex == 1 || $duplex == 2)
set $rx_bytes_min=$das_expected_rx * 60

if $link_up == 1 && $speed_valid && $duplex_valid && $tx_count >= $das_expected_tx_min && $rx_count >= $das_expected_rx && $rx_bytes >= $rx_bytes_min && $rx_test_count == $das_expected_rx && $rx_test_errors == 0 && $rx_last_sequence == $das_expected_rx && $last_result == 0
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
