#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STM32_CUBE_H7_DIR="${STM32_CUBE_H7_DIR:-}"
BUILD_ROOT="${DAS_ETH_RAW_BUILD_DIR:-$ROOT_DIR/build/examples/eth_raw}"
OPENOCD_SCRIPTS="${OPENOCD_SCRIPTS:-/usr/share/openocd/scripts}"

usage() {
  cat <<'USAGE'
Usage:
  scripts/build_and_flash_eth_raw.sh /path/to/STM32CubeH7
  scripts/build_and_flash_eth_raw.sh --stm32h7-root /path/to/STM32CubeH7

Environment overrides:
  DAS_ETH_RAW_BUILD_DIR   Build/install root.
  OPENOCD_SCRIPTS         OpenOCD scripts directory.

After flashing:
  GREEN LED    link up
  YELLOW LED   toggles after each transmitted raw frame
  RED LED      error latched

The firmware broadcasts EtherType 0x88B5 from 02:00:00:00:00:01 once per second.
Capture it on the connected Linux interface with:
  sudo tcpdump -i <iface> -e -XX 'ether proto 0x88b5'
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --stm32h7-root) STM32_CUBE_H7_DIR="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    --*) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    *)
      [[ -z "$STM32_CUBE_H7_DIR" ]] || { echo "Unexpected argument: $1" >&2; exit 2; }
      STM32_CUBE_H7_DIR="$1"
      shift
      ;;
  esac
done

for command in cmake openocd arm-none-eabi-gcc arm-none-eabi-size; do
  command -v "$command" >/dev/null 2>&1 || {
    echo "Missing command: $command" >&2
    exit 2
  }
done

[[ -n "$STM32_CUBE_H7_DIR" ]] || { usage >&2; exit 2; }
STM32_CUBE_H7_DIR="$(cd "$STM32_CUBE_H7_DIR" 2>/dev/null && pwd)" || {
  echo "Invalid STM32CubeH7 root: $STM32_CUBE_H7_DIR" >&2
  exit 2
}

DAS_BUILD_DIR="$BUILD_ROOT/das"
INSTALL_DIR="$BUILD_ROOT/install"
APP_BUILD_DIR="$BUILD_ROOT/app"
rm -rf -- "$BUILD_ROOT"
mkdir -p "$BUILD_ROOT"

echo "=== Build DAS static library ==="
cmake -S "$ROOT_DIR" -B "$DAS_BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/cmake/toolchains/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDAS_DEVICE=nucleo_h755zi_q \
  -DDAS_CORE=cm7 \
  -DSTM32_CUBE_H7_DIR="$STM32_CUBE_H7_DIR" \
  -DDAS_BUILD_HARDWARE_TESTS=OFF \
  -DDAS_BUILD_LINK_TESTS=OFF
cmake --build "$DAS_BUILD_DIR" --parallel
cmake --install "$DAS_BUILD_DIR" --prefix "$INSTALL_DIR"

[[ -s "$INSTALL_DIR/lib/libdas.a" ]] || {
  echo "Installed static library not found: $INSTALL_DIR/lib/libdas.a" >&2
  exit 1
}

echo
echo "=== Build external raw Ethernet consumer ==="
cmake -S "$ROOT_DIR/examples/eth_raw" -B "$APP_BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/cmake/toolchains/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDAS_CORE=cm7 \
  -DCMAKE_PREFIX_PATH="$INSTALL_DIR"
cmake --build "$APP_BUILD_DIR" --parallel

ELF="$APP_BUILD_DIR/das_eth_raw.elf"
[[ -s "$ELF" ]] || {
  echo "Expected example ELF not found: $ELF" >&2
  exit 1
}

DAS_DIR_USED="$(grep '^DAS_DIR:PATH=' "$APP_BUILD_DIR/CMakeCache.txt" | cut -d= -f2- || true)"
case "$DAS_DIR_USED" in
  "$INSTALL_DIR"/*) ;;
  *)
    echo "Example did not resolve DAS from the generated install prefix: ${DAS_DIR_USED:-unknown}" >&2
    exit 1
    ;;
esac

arm-none-eabi-size "$ELF"
echo "Consumer package: $DAS_DIR_USED"
echo "Static library:    $INSTALL_DIR/lib/libdas.a"
echo "Firmware:          $ELF"

echo
echo "=== Flash CM7 with OpenOCD ==="
openocd -s "$OPENOCD_SCRIPTS" \
  -f "$ROOT_DIR/scripts/openocd_h755_dual_core.cfg" \
  -c "init" \
  -c "reset halt" \
  -c "targets stm32h7x.cpu0" \
  -c "program $ELF verify" \
  -c "reset halt" \
  -c "targets stm32h7x.cpu1" \
  -c "halt" \
  -c "targets stm32h7x.cpu0" \
  -c "resume" \
  -c "shutdown"

cat <<'INFO'

Raw Ethernet firmware is running.

Board expectations:
  - Ethernet cable connected to CN14
  - JP6 and JP7 fitted for the NUCLEO-H755ZI-Q Ethernet route
  - GREEN LED on after LAN8742A link negotiation completes
  - YELLOW LED toggles once per transmitted test frame
  - RED LED means the example observed an error

Linux capture:
  sudo tcpdump -i <iface> -e -XX 'ether proto 0x88b5'

Expected frame:
  dst       ff:ff:ff:ff:ff:ff
  src       02:00:00:00:00:01
  EtherType 0x88b5
  payload   "DAS ETH L2 test"

Debugger observables:
  g_das_eth_link_up
  g_das_eth_speed_mbps
  g_das_eth_duplex
  g_das_eth_tx_count
  g_das_eth_rx_count
  g_das_eth_rx_bytes
  g_das_eth_last_result
INFO
