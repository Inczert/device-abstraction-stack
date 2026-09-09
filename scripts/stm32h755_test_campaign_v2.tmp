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
STAMP=""
CAMPAIGN_ROOT=""
LOG_DIR=""
ARCHIVE=""
OPENOCD_LOG=""
SUMMARY=""

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

if (( CLEAN != 0 && SKIP_BUILD != 0 )); then
  echo "--clean and --no-build cannot be used together" >&2
  exit 2
fi

need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing command: $1" >&2; exit 2; }; }
for command in cmake openocd timeout tee grep tar; do need "$command"; done
if command -v gdb-multiarch >/dev/null 2>&1; then GDB_BIN=gdb-multiarch
elif command -v arm-none-eabi-gdb >/dev/null 2>&1; then GDB_BIN=arm-none-eabi-gdb
else echo "Install gdb-multiarch or arm-none-eabi-gdb" >&2; exit 2
fi

if (( SKIP_BUILD == 0 )); then
  [[ -n "$STM32_CUBE_H7_DIR" ]] || { usage >&2; exit 2; }
fi

if (( CLEAN != 0 )); then
  rm -rf -- "$BUILD_DIR"
fi

STAMP="$(date -u +'%Y%m%dT%H%M%SZ')"
CAMPAIGN_ROOT="$BUILD_DIR/campaign"
LOG_DIR="$CAMPAIGN_ROOT/$STAMP"
ARCHIVE="$CAMPAIGN_ROOT/das-stm32h755-campaign-$STAMP.tar.gz"
mkdir -p "$LOG_DIR"
OPENOCD_LOG="$LOG_DIR/openocd.log"
SUMMARY="$LOG_DIR/summary.txt"
BUILD_LOG="$LOG_DIR/build.log"
METADATA="$LOG_DIR/metadata.txt"

cleanup_openocd() {
  if [[ -n "${OPENOCD_PID:-}" ]] && kill -0 "$OPENOCD_PID" >/dev/null 2>&1; then
    kill "$OPENOCD_PID" >/dev/null 2>&1 || true
    wait "$OPENOCD_PID" >/dev/null 2>&1 || true
  fi
  OPENOCD_PID=""
}

finalize() {
  local rc=$?
  local tar_rc=0
  trap - EXIT INT TERM
  set +e
  cleanup_openocd

  if [[ -n "${LOG_DIR:-}" && -d "$LOG_DIR" ]]; then
    {
      echo
      echo "PASS: $PASS_COUNT"
      echo "FAIL: $FAIL_COUNT"
      echo "Exit code: $rc"
      echo "Logs: $LOG_DIR"
    } | tee -a "$SUMMARY"

    tar -czf "$ARCHIVE" -C "$CAMPAIGN_ROOT" "$STAMP"
    tar_rc=$?
    if (( tar_rc == 0 )); then
      echo "Evidence archive: $ARCHIVE"
    else
      echo "Failed to create evidence archive: $ARCHIVE" >&2
      (( rc != 0 )) || rc=$tar_rc
    fi
  fi

  exit "$rc"
}
trap finalize EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

{
  echo "DAS STM32H755 hardware campaign"
  echo "UTC start: $(date -u +'%Y-%m-%dT%H:%M:%SZ')"
  echo "Repository: $ROOT_DIR"
  if command -v git >/dev/null 2>&1; then
    echo "DAS commit: $(git -C "$ROOT_DIR" rev-parse HEAD 2>/dev/null || echo unknown)"
  fi
  echo "Build dir: $BUILD_DIR"
  echo "STM32CubeH7: ${STM32_CUBE_H7_DIR:-not supplied}"
  if [[ -n "$STM32_CUBE_H7_DIR" ]] && command -v git >/dev/null 2>&1; then
    echo "STM32CubeH7 commit: $(git -C "$STM32_CUBE_H7_DIR" rev-parse HEAD 2>/dev/null || echo unknown)"
  fi
  echo "GDB: $GDB_BIN"
  "$GDB_BIN" --version 2>/dev/null | head -n 1 || true
  openocd --version 2>&1 | head -n 1 || true
  cmake --version 2>/dev/null | head -n 1 || true
  if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    arm-none-eabi-gcc --version 2>/dev/null | head -n 1 || true
  fi
  uname -a 2>/dev/null || true
} >"$METADATA"

if (( SKIP_BUILD == 0 )); then
  build_args=(--stm32h7-root "$STM32_CUBE_H7_DIR" --build-dir "$BUILD_DIR")
  set +e
  "$ROOT_DIR/scripts/build_stm32h755.sh" "${build_args[@]}" 2>&1 | tee "$BUILD_LOG"
  BUILD_RC=${PIPESTATUS[0]}
  set -e
  if (( BUILD_RC != 0 )); then
    echo "STM32H755 build failed with exit code $BUILD_RC" >&2
    exit "$BUILD_RC"
  fi
else
  echo "Build skipped; reusing existing hardware-test ELF." | tee "$BUILD_LOG"
fi

ELF="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_hw_test.elf"
MAP_FILE="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_hw_test.map"
[[ -s "$ELF" ]] || { echo "Hardware-test ELF not found: $ELF" >&2; exit 1; }
cp "$ELF" "$LOG_DIR/" 2>/dev/null || true
[[ ! -f "$MAP_FILE" ]] || cp "$MAP_FILE" "$LOG_DIR/"
if command -v arm-none-eabi-size >/dev/null 2>&1; then
  arm-none-eabi-size "$ELF" >"$LOG_DIR/elf-size.txt" 2>&1 || true
fi
if command -v arm-none-eabi-nm >/dev/null 2>&1; then
  arm-none-eabi-nm -n "$ELF" >"$LOG_DIR/symbols.txt" 2>&1 || true
fi

safe_log_name() {
  local name="$1"
  name="${name//[^[:alnum:]._-]/_}"
  while [[ "$name" == *"__"* ]]; do
    name="${name//__/_}"
  done
  name="${name#_}"
  name="${name%_}"
  printf '%s' "$name"
}

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

wait_for_enter() {
  echo
  echo "$1"
  read -r -p "Press ENTER when ready... "
}

record() {
  local name="$1" status="$2"
  printf '%-28s %s\n' "$name" "$status" | tee -a "$SUMMARY"
  if [[ "$status" == PASS ]]; then ((PASS_COUNT += 1)); else ((FAIL_COUNT += 1)); fi
}

run_gdb() {
  local log="$1"; shift
  mkdir -p "$(dirname "$log")"
  set +e
  timeout "${DEBUG_TIMEOUT}s" "$GDB_BIN" -q "$ELF" -batch "$@" 2>&1 | tee "$log"
  local pipe_status=("${PIPESTATUS[@]}")
  local gdb_rc=${pipe_status[0]}
  local tee_rc=${pipe_status[1]}
  set -e
  (( gdb_rc == 0 && tee_rc == 0 )) && grep -q '^RESULT: PASS$' "$log"
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
  echo "Firmware bring-up failed; GPIO/LED cases are skipped. Recovery remains an explicit separate action." >&2
  exit 1
fi

automated_gpio_case() {
  local name="$1" command="$2" expected_flags="$3"
  local log="$LOG_DIR/$(safe_log_name "$name").log"
  if run_gdb "$log" \
      -ex "set \$das_command=$command" \
      -ex "set \$das_expected_flags=$expected_flags" \
      -x "$ROOT_DIR/scripts/gdb/stm32h755_gpio_case.gdb"; then
    record "$name" PASS
  else
    record "$name" FAIL
  fi
}

wait_for_enter "GPIO pull test: leave CN10 D3 / PE13 / pin 10 electrically DISCONNECTED. Remove any jumper or shield drive from that pin."
automated_gpio_case "GPIO pull-up" 7 4
automated_gpio_case "GPIO pull-down" 8 8

wait_for_enter "GPIO loopback test: connect ONE jumper from CN10 D4 / PE14 / pin 8 (output) to CN10 D3 / PE13 / pin 10 (input). Do not connect either pin to 3V3, 5V, or GND."
automated_gpio_case "GPIO loopback low/high" 6 3
automated_gpio_case "GPIO open-drain" 9 48
automated_gpio_case "GPIO EXTI rising/falling" 10 192

visual_case() {
  local name="$1" command="$2" expected_mask="$3" prompt="$4"
  local log="$LOG_DIR/$(safe_log_name "$name").log"
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

run_gdb "$LOG_DIR/final_all_off.log" \
  -ex 'set $das_command=1' \
  -ex 'set $das_expected_mask=0' \
  -x "$ROOT_DIR/scripts/gdb/stm32h755_led_case.gdb" >/dev/null || true

(( FAIL_COUNT == 0 )) || exit 1
