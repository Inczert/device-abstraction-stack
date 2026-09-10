#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STM32_CUBE_H7_DIR="${STM32_CUBE_H7_DIR:-}"
BUILD_ROOT="${DAS_LED_BLINK_BUILD_DIR:-$ROOT_DIR/build/examples/led_blink}"
OPENOCD_SCRIPTS="${OPENOCD_SCRIPTS:-/usr/share/openocd/scripts}"

usage() {
  cat <<'USAGE'
Usage:
  scripts/build_and_flash_led_blink.sh /path/to/STM32CubeH7
  scripts/build_and_flash_led_blink.sh --stm32h7-root /path/to/STM32CubeH7

Environment overrides:
  DAS_LED_BLINK_BUILD_DIR   Build/install root.
  OPENOCD_SCRIPTS           OpenOCD scripts directory.
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
[[ -s "$INSTALL_DIR/lib/cmake/DAS/DASConfig.cmake" ]] || {
  echo "Installed DAS CMake package not found" >&2
  exit 1
}

echo
echo "=== Build external LED blink consumer ==="
cmake -S "$ROOT_DIR/examples/led_blink" -B "$APP_BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/cmake/toolchains/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDAS_CORE=cm7 \
  -DCMAKE_PREFIX_PATH="$INSTALL_DIR"
cmake --build "$APP_BUILD_DIR" --parallel

ELF="$APP_BUILD_DIR/das_led_blink.elf"
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

echo
read -r -p "Is the GREEN user LED blinking at roughly 1 Hz? [y/n]: " answer
case "${answer,,}" in
  y|yes)
    echo "LED blink external-consumer example: PASS"
    ;;
  *)
    echo "LED blink external-consumer example: FAIL" >&2
    exit 1
    ;;
esac
