#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STM32_CUBE_H7_DIR="${STM32_CUBE_H7_DIR:-}"
HARDRT_DIR="${HARDRT_DIR:-}"
BUILD_ROOT="${DAS_HARDRT_UART_BUILD_DIR:-$ROOT_DIR/build/examples/hardrt_uart}"
OPENOCD_SCRIPTS="${OPENOCD_SCRIPTS:-/usr/share/openocd/scripts}"
SERIAL_PORT="${DAS_HARDRT_UART_SERIAL_PORT:-}"
MONITOR=0

usage() {
  cat <<'USAGE'
Usage:
  scripts/build_and_flash_hardrt_uart.sh /path/to/STM32CubeH7 [options]
  scripts/build_and_flash_hardrt_uart.sh --stm32h7-root /path/to/STM32CubeH7 [options]

Options:
  --hardrt-root DIR   HardRT source checkout. Defaults to ../hardrt when present.
  --serial-port DEV   ST-LINK VCP device to report/use, e.g. /dev/ttyACM0.
  --monitor           Attach a simple 115200 8N1 serial monitor after flashing.
  -h, --help          Show help.

Environment overrides:
  HARDRT_DIR                    HardRT source checkout.
  DAS_HARDRT_UART_BUILD_DIR     Build/install root.
  DAS_HARDRT_UART_SERIAL_PORT   ST-LINK VCP device.
  OPENOCD_SCRIPTS               OpenOCD scripts directory.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --stm32h7-root) STM32_CUBE_H7_DIR="$2"; shift 2 ;;
    --hardrt-root) HARDRT_DIR="$2"; shift 2 ;;
    --serial-port) SERIAL_PORT="$2"; shift 2 ;;
    --monitor) MONITOR=1; shift ;;
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

if [[ -z "$HARDRT_DIR" && -f "$ROOT_DIR/../hardrt/CMakeLists.txt" ]]; then
  HARDRT_DIR="$ROOT_DIR/../hardrt"
fi
[[ -n "$HARDRT_DIR" ]] || {
  echo "HardRT checkout not found. Use --hardrt-root DIR or set HARDRT_DIR." >&2
  exit 2
}
HARDRT_DIR="$(cd "$HARDRT_DIR" 2>/dev/null && pwd)" || {
  echo "Invalid HardRT root: $HARDRT_DIR" >&2
  exit 2
}
[[ -f "$HARDRT_DIR/CMakeLists.txt" ]] || {
  echo "HardRT CMakeLists.txt not found under: $HARDRT_DIR" >&2
  exit 2
}

DAS_BUILD_DIR="$BUILD_ROOT/das"
HARDRT_BUILD_DIR="$BUILD_ROOT/hardrt"
DAS_INSTALL_DIR="$BUILD_ROOT/install/das"
HARDRT_INSTALL_DIR="$BUILD_ROOT/install/hardrt"
APP_BUILD_DIR="$BUILD_ROOT/app"

rm -rf -- "$BUILD_ROOT"
mkdir -p "$BUILD_ROOT"

echo "=== Build and install DAS CM7 static library ==="
cmake -S "$ROOT_DIR" -B "$DAS_BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/cmake/toolchains/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDAS_DEVICE=nucleo_h755zi_q \
  -DDAS_CORE=cm7 \
  -DSTM32_CUBE_H7_DIR="$STM32_CUBE_H7_DIR" \
  -DDAS_BUILD_HARDWARE_TESTS=OFF \
  -DDAS_BUILD_LINK_TESTS=OFF
cmake --build "$DAS_BUILD_DIR" --parallel
cmake --install "$DAS_BUILD_DIR" --prefix "$DAS_INSTALL_DIR"

[[ -s "$DAS_INSTALL_DIR/lib/libdas.a" ]] || {
  echo "Installed DAS static library not found: $DAS_INSTALL_DIR/lib/libdas.a" >&2
  exit 1
}
[[ -s "$DAS_INSTALL_DIR/lib/cmake/DAS/DASConfig.cmake" ]] || {
  echo "Installed DAS CMake package not found" >&2
  exit 1
}

echo
echo "=== Build and install HardRT Cortex-M static library ==="
cmake -S "$HARDRT_DIR" -B "$HARDRT_BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/cmake/toolchains/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDAS_CORE=cm7 \
  -DHARDRT_PORT=cortex_m \
  -DHARDRT_BUILD_EXAMPLES=OFF \
  -DHARDRT_BUILD_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX="$HARDRT_INSTALL_DIR"
cmake --build "$HARDRT_BUILD_DIR" --parallel
cmake --install "$HARDRT_BUILD_DIR"

[[ -s "$HARDRT_INSTALL_DIR/lib/libhardrt.a" ]] || {
  echo "Installed HardRT static library not found: $HARDRT_INSTALL_DIR/lib/libhardrt.a" >&2
  exit 1
}
[[ -s "$HARDRT_INSTALL_DIR/lib/cmake/HardRT/HardRTConfig.cmake" ]] || {
  echo "Installed HardRT CMake package not found" >&2
  exit 1
}

echo
echo "=== Build external HardRT + DAS UART consumer ==="
cmake -S "$ROOT_DIR/examples/hardrt_uart" -B "$APP_BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/cmake/toolchains/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDAS_CORE=cm7 \
  -DCMAKE_PREFIX_PATH="$HARDRT_INSTALL_DIR;$DAS_INSTALL_DIR"
cmake --build "$APP_BUILD_DIR" --parallel

ELF="$APP_BUILD_DIR/das_hardrt_uart.elf"
[[ -s "$ELF" ]] || {
  echo "Expected example ELF not found: $ELF" >&2
  exit 1
}

DAS_DIR_USED="$(grep '^DAS_DIR:PATH=' "$APP_BUILD_DIR/CMakeCache.txt" | cut -d= -f2- || true)"
HARDRT_DIR_USED="$(grep '^HardRT_DIR:PATH=' "$APP_BUILD_DIR/CMakeCache.txt" | cut -d= -f2- || true)"
case "$DAS_DIR_USED" in
  "$DAS_INSTALL_DIR"/*) ;;
  *)
    echo "Example did not resolve DAS from the generated install prefix: ${DAS_DIR_USED:-unknown}" >&2
    exit 1
    ;;
esac
case "$HARDRT_DIR_USED" in
  "$HARDRT_INSTALL_DIR"/*) ;;
  *)
    echo "Example did not resolve HardRT from the generated install prefix: ${HARDRT_DIR_USED:-unknown}" >&2
    exit 1
    ;;
esac

arm-none-eabi-size "$ELF"
echo "DAS package:        $DAS_DIR_USED"
echo "HardRT package:     $HARDRT_DIR_USED"
echo "DAS static library: $DAS_INSTALL_DIR/lib/libdas.a"
echo "HardRT library:     $HARDRT_INSTALL_DIR/lib/libhardrt.a"
echo "Firmware:           $ELF"

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

find_stlink_vcp() {
  local candidate base
  for candidate in /dev/serial/by-id/*; do
    [[ -e "$candidate" ]] || continue
    base="$(basename "$candidate")"
    case "$base" in
      *STLink*|*STLINK*|*ST-LINK*|*STMicroelectronics*)
        printf '%s\n' "$candidate"
        return 0
        ;;
    esac
  done
  return 1
}

if [[ -z "$SERIAL_PORT" ]]; then
  SERIAL_PORT="$(find_stlink_vcp || true)"
fi

echo
echo "=== Runtime ==="
echo "GREEN LED: changes state every 250 ms"
echo "UART:      USART3 via ST-LINK VCP, PD8 TX / PD9 RX, 115200 8N1"
echo "Message:   HardRT + DAS alive (once per second)"
if [[ -n "$SERIAL_PORT" ]]; then
  echo "VCP:       $SERIAL_PORT"
else
  echo "VCP:       not auto-detected; inspect /dev/serial/by-id/ or pass --serial-port"
fi

if (( MONITOR != 0 )); then
  [[ -n "$SERIAL_PORT" ]] || {
    echo "Cannot monitor UART without a serial device." >&2
    exit 1
  }
  command -v stty >/dev/null 2>&1 || {
    echo "Missing command: stty" >&2
    exit 2
  }
  [[ -e "$SERIAL_PORT" ]] || {
    echo "Serial device does not exist: $SERIAL_PORT" >&2
    exit 1
  }
  echo
echo "=== Serial monitor: $SERIAL_PORT (Ctrl-C to exit) ==="
  stty -F "$SERIAL_PORT" 115200 cs8 -cstopb -parenb -ixon -ixoff -crtscts raw -echo
  exec cat "$SERIAL_PORT"
fi
