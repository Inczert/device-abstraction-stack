#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STM32_CUBE_H7_DIR="${1:-${STM32_CUBE_H7_DIR:-}}"
BUILD_ROOT="${DAS_UART_BUILD_DIR:-$ROOT_DIR/build/stm32h755-uart}"
OPENOCD_SCRIPTS="${OPENOCD_SCRIPTS:-/usr/share/openocd/scripts}"
OPENOCD_PID=""

if [[ -z "$STM32_CUBE_H7_DIR" ]]; then
  echo "Usage: $0 /path/to/STM32CubeH7" >&2
  exit 2
fi

for command in cmake openocd timeout tee; do
  command -v "$command" >/dev/null 2>&1 || { echo "Missing command: $command" >&2; exit 2; }
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

"$ROOT_DIR/scripts/build_stm32h755.sh" \
  --stm32h7-root "$STM32_CUBE_H7_DIR" \
  --build-dir "$BUILD_ROOT/cm7" \
  --core cm7 \
  --clean

"$ROOT_DIR/scripts/build_stm32h755.sh" \
  --stm32h7-root "$STM32_CUBE_H7_DIR" \
  --build-dir "$BUILD_ROOT/cm4" \
  --core cm4 \
  --clean

CM7_ELF="$BUILD_ROOT/cm7/tests/hardware/stm32h755/das_stm32h755_uart_test.elf"
CM4_ELF="$BUILD_ROOT/cm4/tests/hardware/stm32h755/das_stm32h755_uart_test.elf"
[[ -s "$CM7_ELF" && -s "$CM4_ELF" ]] || {
  echo "UART qualification ELF missing after build" >&2
  exit 1
}

echo
echo "UART loopback wiring: connect ONE jumper between Arduino D1/TX (PB6) and Arduino D0/RX (PB7)."
echo "Do not connect either signal to 3V3, 5V, or GND."
read -r -p "Press ENTER when the D1-to-D0 jumper is connected... "

OPENOCD_LOG="$BUILD_ROOT/logs/openocd.log"
openocd -s "$OPENOCD_SCRIPTS" \
  -f "$ROOT_DIR/scripts/openocd_h755_dual_core.cfg" \
  -c "init; reset halt" >"$OPENOCD_LOG" 2>&1 &
OPENOCD_PID=$!

for ((attempt = 0; attempt < 150; ++attempt)); do
  if grep -q "Listening on port 3333 for gdb connections" "$OPENOCD_LOG" 2>/dev/null && \
     grep -q "Listening on port 3334 for gdb connections" "$OPENOCD_LOG" 2>/dev/null; then
    break
  fi
  if ! kill -0 "$OPENOCD_PID" >/dev/null 2>&1; then
    cat "$OPENOCD_LOG" >&2
    exit 1
  fi
  sleep 0.1
done

run_case() {
  local core="$1" elf="$2" port="$3" log="$4"
  set +e
  timeout 30s "$GDB_BIN" -q "$elf" -batch \
    -ex "target extended-remote :$port" \
    -x "$ROOT_DIR/scripts/gdb/stm32h755_uart_case.gdb" 2>&1 | tee "$log"
  local rc=${PIPESTATUS[0]}
  set -e
  if (( rc != 0 )) || ! grep -q '^RESULT: PASS$' "$log"; then
    echo "$core UART loopback: FAIL"
    return 1
  fi
  echo "$core UART loopback: PASS"
}

run_case CM7 "$CM7_ELF" 3333 "$BUILD_ROOT/logs/cm7-uart.log"
run_case CM4 "$CM4_ELF" 3334 "$BUILD_ROOT/logs/cm4-uart.log"

echo
echo "UART qualification: 2/2 PASS"
echo "Logs: $BUILD_ROOT/logs"
