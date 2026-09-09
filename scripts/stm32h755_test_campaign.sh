#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STM32_CUBE_H7_DIR="${STM32_CUBE_H7_DIR:-}"
BUILD_DIR="${DAS_STM32_BUILD_DIR:-$ROOT_DIR/build/stm32h755}"
OPENOCD_SCRIPTS="${OPENOCD_SCRIPTS:-/usr/share/openocd/scripts}"
DEBUG_TIMEOUT=30
CLEAN=0
SKIP_BUILD=0
OPENOCD_PID=""
GDB_BIN=""
PASS_COUNT=0
FAIL_COUNT=0

usage() {
  cat <<'USAGE'
Usage:
  scripts/stm32h755_test_campaign.sh /path/to/STM32CubeH7 [options]
  scripts/stm32h755_test_campaign.sh --stm32h7-root /path/to/STM32CubeH7 [options]

Options:
  --stm32h7-root DIR      STM32CubeH7 checkout root.
  --build-dir DIR         Build directory (default: build/stm32h755).
  --openocd-scripts DIR   OpenOCD scripts directory.
  --debug-timeout SEC     GDB timeout per case (default: 30).
  --clean                 Clean before building.
  --no-build              Reuse the existing hardware-test ELF.
  -h, --help              Show help.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --stm32h7-root) STM32_CUBE_H7_DIR="$2"; shift 2 ;;
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    --openocd-scripts) OPENOCD_SCRIPTS="$2"; shift 2 ;;
    --debug-timeout) DEBUG_TIMEOUT="$2"; shift 2 ;;
    --clean) CLEAN=1; shift ;;
    --no-build) SKIP_BUILD=1; shift ;;
    -h|--help) usage; exit 0 ;;
    --*) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    *)
      [[ -z "$STM32_CUBE_H7_DIR" ]] || { echo "Unexpected argument: $1" >&2; exit 2; }
      STM32_CUBE_H7_DIR="$1"; shift ;;
  esac
done

[[ "$DEBUG_TIMEOUT" =~ ^[0-9]+$ ]] && (( DEBUG_TIMEOUT > 0 )) || {
  echo "--debug-timeout must be a positive integer" >&2
  exit 2
}

need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing command: $1" >&2; exit 2; }; }
for command in cmake openocd timeout tee grep; do need "$command"; done
if command -v gdb-multiarch >/dev/null 2>&1; then GDB_BIN=gdb-multiarch
elif command -v arm-none-eabi-gdb >/dev/null 2>&1; then GDB_BIN=arm-none-eabi-gdb
else echo "Install gdb-multiarch or arm-none-eabi-gdb" >&2; exit 2
fi

if (( SKIP_BUILD == 0 )); then
  [[ -n "$STM32_CUBE_H7_DIR" ]] || { usage >&2; exit 2; }
  build_args=(--stm32h7-root "$STM32_CUBE_H7_DIR" --build-dir "$BUILD_DIR")
  (( CLEAN == 0 )) || build_args+=(--clean)
  "$ROOT_DIR/scripts/build_stm32h755.sh" "${build_args[@]}"
fi

ELF="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_hw_test.elf"
[[ -s "$ELF" ]] || { echo "Hardware-test ELF not found: $ELF" >&2; exit 1; }

STAMP="$(date -u +'%Y%m%dT%H%M%SZ')"
LOG_DIR="$BUILD_DIR/campaign/$STAMP"
mkdir -p "$LOG_DIR"
OPENOCD_LOG="$LOG_DIR/openocd.log"
SUMMARY="$LOG_DIR/summary.txt"

cleanup_openocd() {
  if [[ -n "${OPENOCD_PID:-}" ]] && kill -0 "$OPENOCD_PID" >/dev/null 2>&1; then
    kill "$OPENOCD_PID" >/dev/null 2>&1 || true
    wait "$OPENOCD_PID" >/dev/null 2>&1 || true
  fi
  OPENOCD_PID=""
}
trap cleanup_openocd EXIT INT TERM

yes_no() {
  local answer
  while true; do
    read -r -p "$1 [y/n]: " answer
    case "${answer,,}" in
      y|yes) return 0 ;;
      n|no) return 1 ;;
      *) echo "Please answer y or n." ;;
    esac
  done
}

record() {
  local name="$1" status="$2"
  printf '%-28s %s\n' "$name" "$status" | tee -a "$SUMMARY"
  if [[ "$status" == PASS ]]; then ((PASS_COUNT += 1)); else ((FAIL_COUNT += 1)); fi
}

run_gdb() {
  local log="$1"; shift
  set +e
  timeout "${DEBUG_TIMEOUT}s" "$GDB_BIN" -q "$ELF" -batch "$@" 2>&1 | tee "$log"
  local rc=${PIPESTATUS[0]}
  set -e
  (( rc == 0 )) && grep -q '^RESULT: PASS$' "$log"
}

echo "Starting OpenOCD..."
openocd -s "$OPENOCD_SCRIPTS" \
  -f "$ROOT_DIR/scripts/openocd_h755.cfg" \
  -c "init; reset halt" >"$OPENOCD_LOG" 2>&1 &
OPENOCD_PID=$!
for ((attempt = 0; attempt < 100; ++attempt)); do
  grep -q "Listening on port 3333 for gdb connections" "$OPENOCD_LOG" 2>/dev/null && break
  if ! kill -0 "$OPENOCD_PID" >/dev/null 2>&1; then
    cat "$OPENOCD_LOG" >&2
    echo "OpenOCD exited before GDB became ready" >&2
    exit 1
  fi
  sleep 0.1
done
if ! grep -q "Listening on port 3333 for gdb connections" "$OPENOCD_LOG"; then
  cat "$OPENOCD_LOG" >&2
  echo "OpenOCD GDB server timeout" >&2
  exit 1
fi

if run_gdb "$LOG_DIR/board_probe.log" -x "$ROOT_DIR/scripts/gdb/stm32h755_probe.gdb"; then
  record "Board/OpenOCD probe" PASS
else
  record "Board/OpenOCD probe" FAIL
  echo "Non-destructive board probe failed; nothing was flashed. If the target will not attach, run scripts/stm32h755_recover.sh." >&2
  exit 1
fi

if run_gdb "$LOG_DIR/flash_probe.log" -x "$ROOT_DIR/scripts/gdb/stm32h755_flash_probe.gdb"; then
  record "CMSIS/GPIO bring-up" PASS
else
  record "CMSIS/GPIO bring-up" FAIL
  echo "Firmware bring-up failed; LED cases are skipped. Recovery remains an explicit separate action." >&2
  exit 1
fi

visual_case() {
  local name="$1" command="$2" expected_mask="$3" prompt="$4"
  local log="$LOG_DIR/${name// /_}.log"
  local automated=FAIL visual=FAIL

  if run_gdb "$log" \
      -ex "set \$das_command=$command" \
      -ex "set \$das_expected_mask=$expected_mask" \
      -x "$ROOT_DIR/scripts/gdb/stm32h755_led_case.gdb"; then
    automated=PASS
  fi

  if [[ "$automated" == PASS ]] && yes_no "$prompt"; then
    visual=PASS
  fi

  if [[ "$automated" == PASS && "$visual" == PASS ]]; then
    record "$name" PASS
  else
    record "$name" FAIL
  fi
}

visual_case "LED all off" 1 0 "Are green, yellow, and red user LEDs all OFF"
visual_case "LED green only" 2 1 "Is only the GREEN user LED ON"
visual_case "LED yellow only" 3 2 "Is only the YELLOW user LED ON"
visual_case "LED red only" 4 4 "Is only the RED user LED ON"
visual_case "LED all blink" 5 0 "Are all three user LEDs visibly BLINKING together"

# Leave the board in a quiet state without adding another human acceptance point.
run_gdb "$LOG_DIR/final_all_off.log" \
  -ex 'set $das_command=1' \
  -ex 'set $das_expected_mask=0' \
  -x "$ROOT_DIR/scripts/gdb/stm32h755_led_case.gdb" >/dev/null || true

{
  echo
  echo "PASS: $PASS_COUNT"
  echo "FAIL: $FAIL_COUNT"
  echo "Logs: $LOG_DIR"
} | tee -a "$SUMMARY"

(( FAIL_COUNT == 0 )) || exit 1
