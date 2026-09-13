#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STM32_CUBE_H7_DIR="${STM32_CUBE_H7_DIR:-}"
IFACE="${DAS_ETH_IFACE:-${IFACE:-enp0s31f6}}"
BUILD_ROOT="${DAS_ETH_TEST_BUILD_DIR:-$ROOT_DIR/build/stm32h755-eth}"
OPENOCD_SCRIPTS="${OPENOCD_SCRIPTS:-/usr/share/openocd/scripts}"
TX_COUNT="${DAS_ETH_TX_CAPTURE_COUNT:-5}"
RX_COUNT="${DAS_ETH_RX_FRAME_COUNT:-64}"
OPENOCD_PID=""

usage() {
  cat <<'USAGE'
Usage:
  scripts/stm32h755_eth_test.sh /path/to/STM32CubeH7 [--iface <linux-interface>]
  scripts/stm32h755_eth_test.sh --stm32h7-root /path/to/STM32CubeH7 [--iface <linux-interface>]

Options:
  --iface IFACE          Linux Ethernet interface connected to board CN14.
                         Default: enp0s31f6

Environment alternatives:
  STM32_CUBE_H7_DIR=/path/to/STM32CubeH7
  DAS_ETH_IFACE=<linux-interface>
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --stm32h7-root)
      [[ $# -ge 2 ]] || { echo "Missing value for --stm32h7-root" >&2; exit 2; }
      STM32_CUBE_H7_DIR="$2"
      shift 2
      ;;
    --iface)
      [[ $# -ge 2 ]] || { echo "Missing value for --iface" >&2; exit 2; }
      IFACE="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --*)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
    *)
      if [[ -z "$STM32_CUBE_H7_DIR" ]]; then
        STM32_CUBE_H7_DIR="$1"
        shift
      else
        echo "Unexpected argument: $1" >&2
        exit 2
      fi
      ;;
  esac
done

[[ -n "$STM32_CUBE_H7_DIR" ]] || { usage >&2; exit 2; }
STM32_CUBE_H7_DIR="$(cd "$STM32_CUBE_H7_DIR" 2>/dev/null && pwd)" || {
  echo "Invalid STM32CubeH7 root: $STM32_CUBE_H7_DIR" >&2
  exit 2
}
[[ -d "/sys/class/net/$IFACE" ]] || {
  echo "Network interface not found: $IFACE" >&2
  echo "Use --iface <linux-interface> or DAS_ETH_IFACE to override the default." >&2
  ip -br link >&2 || true
  exit 2
}

for command in openocd python3 ip sudo timeout grep; do
  command -v "$command" >/dev/null 2>&1 || {
    echo "Missing command: $command" >&2
    exit 2
  }
done

if command -v gdb-multiarch >/dev/null 2>&1; then
  GDB_BIN=gdb-multiarch
elif command -v arm-none-eabi-gdb >/dev/null 2>&1; then
  GDB_BIN=arm-none-eabi-gdb
else
  echo "Install gdb-multiarch or arm-none-eabi-gdb" >&2
  exit 2
fi

cleanup() {
  if [[ -n "$OPENOCD_PID" ]] && kill -0 "$OPENOCD_PID" >/dev/null 2>&1; then
    kill "$OPENOCD_PID" >/dev/null 2>&1 || true
    wait "$OPENOCD_PID" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT INT TERM

rm -rf "$BUILD_ROOT"
mkdir -p "$BUILD_ROOT/logs"

sudo -v
sudo ip link set "$IFACE" up

{
  echo "DAS STM32H755 Ethernet Layer-2 qualification"
  echo "UTC start: $(date -u +'%Y-%m-%dT%H:%M:%SZ')"
  echo "DAS commit: $(git -C "$ROOT_DIR" rev-parse HEAD 2>/dev/null || echo unknown)"
  echo "STM32CubeH7 commit: $(git -C "$STM32_CUBE_H7_DIR" rev-parse HEAD 2>/dev/null || echo unknown)"
  echo "Host interface: $IFACE"
  echo "Host MAC: $(cat "/sys/class/net/$IFACE/address")"
  echo "DAS MAC: 02:00:00:00:00:01"
  echo "TX validation frames: $TX_COUNT"
  echo "RX integrity frames: $RX_COUNT"
} >"$BUILD_ROOT/logs/metadata.txt"

BUILD_LOG="$BUILD_ROOT/logs/build-flash.log"
set +e
DAS_ETH_RAW_BUILD_DIR="$BUILD_ROOT/firmware" \
  "$ROOT_DIR/scripts/build_and_flash_eth_raw.sh" "$STM32_CUBE_H7_DIR" \
  >"$BUILD_LOG" 2>&1
BUILD_RC=$?
set -e
cat "$BUILD_LOG"
(( BUILD_RC == 0 )) || { echo "Ethernet build/flash: FAIL"; exit 1; }

echo
echo "=== Physical carrier ==="
CARRIER=0
for ((attempt = 0; attempt < 100; ++attempt)); do
  CARRIER="$(cat "/sys/class/net/$IFACE/carrier" 2>/dev/null || echo 0)"
  [[ "$CARRIER" == "1" ]] && break
  sleep 0.1
done
if [[ "$CARRIER" != "1" ]]; then
  ip -br link show dev "$IFACE" || true
  echo "Physical carrier: FAIL"
  echo "Check CN14 cable and JP6/JP7." >&2
  exit 1
fi
echo "Physical carrier: PASS"
ip -br link show dev "$IFACE" || true

echo
echo "=== Bidirectional raw Ethernet traffic ==="
TRAFFIC_LOG="$BUILD_ROOT/logs/traffic.log"
set +e
sudo python3 "$ROOT_DIR/scripts/host/stm32h755_eth_traffic.py" \
  --iface "$IFACE" \
  --tx-count "$TX_COUNT" \
  --rx-count "$RX_COUNT" \
  >"$TRAFFIC_LOG" 2>&1
TRAFFIC_RC=$?
set -e
cat "$TRAFFIC_LOG"
if (( TRAFFIC_RC != 0 )) || ! grep -q '^TRAFFIC_RESULT: PASS$' "$TRAFFIC_LOG"; then
  echo "Raw Ethernet traffic phase: FAIL"
  exit 1
fi

echo
echo "=== STM32 evidence ==="
ELF="$BUILD_ROOT/firmware/app/das_eth_raw.elf"
[[ -s "$ELF" ]] || { echo "Firmware ELF missing: $ELF" >&2; exit 1; }

OPENOCD_LOG="$BUILD_ROOT/logs/openocd-state.log"
openocd -s "$OPENOCD_SCRIPTS" \
  -f "$ROOT_DIR/scripts/openocd_h755_dual_core.cfg" \
  -c "init" \
  -c "targets stm32h7x.cpu0" >"$OPENOCD_LOG" 2>&1 &
OPENOCD_PID=$!

for ((attempt = 0; attempt < 150; ++attempt)); do
  grep -q "Listening on port 3333 for gdb connections" "$OPENOCD_LOG" 2>/dev/null && break
  if ! kill -0 "$OPENOCD_PID" >/dev/null 2>&1; then
    cat "$OPENOCD_LOG" >&2
    echo "OpenOCD state server: FAIL" >&2
    exit 1
  fi
  sleep 0.1
done
if ! grep -q "Listening on port 3333 for gdb connections" "$OPENOCD_LOG"; then
  cat "$OPENOCD_LOG" >&2
  echo "OpenOCD state server timeout" >&2
  exit 1
fi

GDB_LOG="$BUILD_ROOT/logs/gdb-state.log"
set +e
timeout 20s "$GDB_BIN" -q "$ELF" -batch \
  -ex "target extended-remote :3333" \
  -ex "set \$das_expected_rx=$RX_COUNT" \
  -ex "set \$das_expected_tx_min=$TX_COUNT" \
  -x "$ROOT_DIR/scripts/gdb/stm32h755_eth_state.gdb" \
  >"$GDB_LOG" 2>&1
GDB_RC=$?
set -e
cat "$GDB_LOG"

if (( GDB_RC != 0 )) || ! grep -q '^RESULT: PASS$' "$GDB_LOG"; then
  echo
  echo "============================================================"
  echo "STM32H755 ETHERNET LAYER-2 QUALIFICATION: FAIL"
  echo "============================================================"
  echo "Logs: $BUILD_ROOT/logs"
  exit 1
fi

echo
echo "============================================================"
echo "STM32H755 ETHERNET LAYER-2 QUALIFICATION: PASS"
echo "============================================================"
echo "Physical carrier:                  PASS"
echo "STM32 -> PC validated raw frames: $TX_COUNT/$TX_COUNT PASS"
echo "PC -> STM32 integrity frames:      $RX_COUNT/$RX_COUNT PASS"
echo "PHY/MAC/DMA/cache evidence:        PASS"
echo "Logs: $BUILD_ROOT/logs"
