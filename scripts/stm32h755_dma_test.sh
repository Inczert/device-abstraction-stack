#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STM32_CUBE_H7_DIR="${1:-${STM32_CUBE_H7_DIR:-}}"
BUILD_ROOT="${DAS_DMA_BUILD_DIR:-$ROOT_DIR/build/stm32h755-dma}"
OPENOCD_SCRIPTS="${OPENOCD_SCRIPTS:-/usr/share/openocd/scripts}"
OPENOCD_PID=""

if [[ -z "$STM32_CUBE_H7_DIR" ]]; then
  echo "Usage: $0 /path/to/STM32CubeH7" >&2
  exit 2
fi

for command in cmake openocd timeout tee grep; do
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

{
  echo "DAS STM32H755 focused DMA/cache qualification"
  echo "UTC start: $(date -u +'%Y-%m-%dT%H:%M:%SZ')"
  echo "Repository: $ROOT_DIR"
  if command -v git >/dev/null 2>&1; then
    echo "DAS commit: $(git -C "$ROOT_DIR" rev-parse HEAD 2>/dev/null || echo unknown)"
  fi
  echo "STM32CubeH7: $STM32_CUBE_H7_DIR"
  if command -v git >/dev/null 2>&1; then
    echo "STM32CubeH7 commit: $(git -C "$STM32_CUBE_H7_DIR" rev-parse HEAD 2>/dev/null || echo unknown)"
  fi
  echo "Memory DMA: CM7 AXI SRAM / CM4 D2 SRAM through each core's normal linker placement"
  echo "Peripheral DMA fixture: Arduino SPI MOSI/PB5 -> MISO/PA6"
} >"$BUILD_ROOT/logs/metadata.txt"

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

CM7_ELF="$BUILD_ROOT/cm7/tests/hardware/stm32h755/das_stm32h755_dma_test.elf"
CM4_ELF="$BUILD_ROOT/cm4/tests/hardware/stm32h755/das_stm32h755_dma_test.elf"
[[ -s "$CM7_ELF" && -s "$CM4_ELF" ]] || {
  echo "DMA qualification ELF missing after build" >&2
  exit 1
}

echo
echo "DMA peripheral test reuses the qualified SPI fixture."
echo "Connect ONE jumper: Arduino D11 / MOSI / PB5 <-> Arduino D12 / MISO / PA6."
echo "Leave D13 / SCK / PA5 and D10 / CS / PD14 otherwise unconnected."
echo "Do not connect any signal pin to 3V3, 5V, or GND."
read -r -p "Press ENTER when the MOSI-to-MISO jumper is connected... "

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

if ! grep -q "Listening on port 3333 for gdb connections" "$OPENOCD_LOG" || \
   ! grep -q "Listening on port 3334 for gdb connections" "$OPENOCD_LOG"; then
  cat "$OPENOCD_LOG" >&2
  echo "Dual-core OpenOCD GDB server timeout" >&2
  exit 1
fi

run_case() {
  local core="$1" elf="$2" port="$3" expected_cache="$4" expected_hz="$5" log="$6"
  set +e
  timeout 30s "$GDB_BIN" -q "$elf" -batch \
    -ex "target extended-remote :$port" \
    -ex "set \$das_expected_cache=$expected_cache" \
    -ex "set \$das_expected_core_hz=$expected_hz" \
    -x "$ROOT_DIR/scripts/gdb/stm32h755_dma_case.gdb" 2>&1 | tee "$log"
  local rc=${PIPESTATUS[0]}
  set -e
  if (( rc != 0 )) || ! grep -q '^RESULT: PASS$' "$log"; then
    echo "$core DMA/cache: FAIL"
    return 1
  fi
  echo "$core DMA/cache: PASS"
}

run_case CM7 "$CM7_ELF" 3333 1 400000000 "$BUILD_ROOT/logs/cm7-dma.log"
run_case CM4 "$CM4_ELF" 3334 0 64000000 "$BUILD_ROOT/logs/cm4-dma.log"

echo
echo "DMA/cache qualification: 2/2 PASS"
echo "Logs: $BUILD_ROOT/logs"
