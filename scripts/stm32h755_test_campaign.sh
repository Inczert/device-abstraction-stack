#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STM32_CUBE_H7_DIR="${STM32_CUBE_H7_DIR:-}"
BUILD_DIR="${DAS_STM32_BUILD_DIR:-$ROOT_DIR/build/stm32h755}"
CM4_BUILD_DIR=""
CUSTOM_BUILD_DIR=""
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
  --no-build              Reuse existing CM7/CM4 hardware, clock, and custom-link ELFs.
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
for command in cmake openocd timeout tee grep tar arm-none-eabi-nm; do need "$command"; done
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

CM4_BUILD_DIR="$BUILD_DIR/cm4-hw"
CUSTOM_BUILD_DIR="$BUILD_DIR/custom-link"

STAMP="$(date -u +'%Y%m%dT%H%M%SZ')"
CAMPAIGN_ROOT="$BUILD_DIR/campaign"
LOG_DIR="$CAMPAIGN_ROOT/$STAMP"
ARCHIVE="$CAMPAIGN_ROOT/das-stm32h755-campaign-$STAMP.tar.gz"
mkdir -p "$LOG_DIR"
OPENOCD_LOG="$LOG_DIR/openocd-dual-core.log"
SUMMARY="$LOG_DIR/summary.txt"
BUILD_LOG="$LOG_DIR/cm7_build.log"
CM4_BUILD_LOG="$LOG_DIR/cm4_build.log"
CUSTOM_BUILD_LOG="$LOG_DIR/custom_link_build.log"
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
  echo "DAS STM32H755 dual-core hardware campaign"
  echo "UTC start: $(date -u +'%Y-%m-%dT%H:%M:%SZ')"
  echo "Repository: $ROOT_DIR"
  if command -v git >/dev/null 2>&1; then
    echo "DAS commit: $(git -C "$ROOT_DIR" rev-parse HEAD 2>/dev/null || echo unknown)"
  fi
  echo "CM7 hardware build dir: $BUILD_DIR"
  echo "CM4 hardware build dir: $CM4_BUILD_DIR"
  echo "Custom linker build dir: $CUSTOM_BUILD_DIR"
  echo "STM32CubeH7: ${STM32_CUBE_H7_DIR:-not supplied}"
  if [[ -n "$STM32_CUBE_H7_DIR" ]] && command -v git >/dev/null 2>&1; then
    echo "STM32CubeH7 commit: $(git -C "$STM32_CUBE_H7_DIR" rev-parse HEAD 2>/dev/null || echo unknown)"
  fi
  echo "CM7 default linker: $ROOT_DIR/cmake/targets/stm32h755_cm7.ld"
  echo "CM4 default linker: $ROOT_DIR/cmake/targets/stm32h755_cm4.ld"
  echo "Custom linker fixture: $ROOT_DIR/tests/link/stm32h755/custom_cm7.ld"
  echo "OpenOCD dual-core config: $ROOT_DIR/scripts/openocd_h755_dual_core.cfg"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$ROOT_DIR/cmake/targets/stm32h755_cm7.ld" 2>/dev/null || true
    sha256sum "$ROOT_DIR/cmake/targets/stm32h755_cm4.ld" 2>/dev/null || true
    sha256sum "$ROOT_DIR/tests/link/stm32h755/custom_cm7.ld" 2>/dev/null || true
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
  set +e
  "$ROOT_DIR/scripts/build_stm32h755.sh" \
    --stm32h7-root "$STM32_CUBE_H7_DIR" \
    --build-dir "$BUILD_DIR" \
    --core cm7 2>&1 | tee "$BUILD_LOG"
  CM7_BUILD_RC=${PIPESTATUS[0]}
  set -e
  if (( CM7_BUILD_RC != 0 )); then
    echo "STM32H755 CM7 hardware build failed with exit code $CM7_BUILD_RC" >&2
    exit "$CM7_BUILD_RC"
  fi

  set +e
  "$ROOT_DIR/scripts/build_stm32h755.sh" \
    --stm32h7-root "$STM32_CUBE_H7_DIR" \
    --build-dir "$CM4_BUILD_DIR" \
    --core cm4 2>&1 | tee "$CM4_BUILD_LOG"
  CM4_BUILD_RC=${PIPESTATUS[0]}
  set -e
  if (( CM4_BUILD_RC != 0 )); then
    echo "STM32H755 CM4 hardware build failed with exit code $CM4_BUILD_RC" >&2
    exit "$CM4_BUILD_RC"
  fi

  set +e
  {
    cmake -S "$ROOT_DIR" -B "$CUSTOM_BUILD_DIR" \
      -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/cmake/toolchains/arm-none-eabi.cmake" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DDAS_DEVICE=nucleo_h755zi_q \
      -DDAS_CORE=cm7 \
      -DDAS_LINKER_SCRIPT="$ROOT_DIR/tests/link/stm32h755/custom_cm7.ld" \
      -DSTM32_CUBE_H7_DIR="$STM32_CUBE_H7_DIR" \
      -DDAS_BUILD_LINK_TESTS=ON \
      -DDAS_BUILD_HARDWARE_TESTS=OFF
    cmake --build "$CUSTOM_BUILD_DIR" --target das_stm32h755_link_test --parallel
  } 2>&1 | tee "$CUSTOM_BUILD_LOG"
  CUSTOM_BUILD_RC=${PIPESTATUS[0]}
  set -e
  if (( CUSTOM_BUILD_RC != 0 )); then
    echo "STM32H755 custom-linker build failed with exit code $CUSTOM_BUILD_RC" >&2
    exit "$CUSTOM_BUILD_RC"
  fi
else
  echo "Build skipped; reusing existing CM7/CM4 hardware, clock, and custom-link ELFs." | tee "$BUILD_LOG"
fi

CM7_ELF="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_hw_test.elf"
CLOCK_ELF="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_clock_test.elf"
CLOCK_MAP="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_clock_test.map"
CM7_MAP="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_hw_test.map"
CM4_ELF="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_hw_test.elf"
CM4_MAP="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_hw_test.map"
CUSTOM_ELF="$CUSTOM_BUILD_DIR/tests/link/stm32h755/das_stm32h755_link_test.elf"
CUSTOM_MAP="$CUSTOM_BUILD_DIR/tests/link/stm32h755/das_stm32h755_link_test.map"

for path in "$CM7_ELF" "$CM7_MAP" "$CLOCK_ELF" "$CLOCK_MAP" "$CM4_ELF" "$CM4_MAP" "$CUSTOM_ELF" "$CUSTOM_MAP"; do
  [[ -s "$path" ]] || { echo "Expected campaign artifact not found: $path" >&2; exit 1; }
done

cp "$CM7_ELF" "$LOG_DIR/das_stm32h755_cm7_hw_test.elf"
cp "$CM7_MAP" "$LOG_DIR/das_stm32h755_cm7_hw_test.map"
cp "$CLOCK_ELF" "$LOG_DIR/das_stm32h755_cm7_clock_test.elf"
cp "$CLOCK_MAP" "$LOG_DIR/das_stm32h755_cm7_clock_test.map"
cp "$CM4_ELF" "$LOG_DIR/das_stm32h755_cm4_hw_test.elf"
cp "$CM4_MAP" "$LOG_DIR/das_stm32h755_cm4_hw_test.map"
cp "$CUSTOM_ELF" "$LOG_DIR/das_stm32h755_custom_link_test.elf"
cp "$CUSTOM_MAP" "$LOG_DIR/das_stm32h755_custom_link_test.map"
cp "$ROOT_DIR/cmake/targets/stm32h755_cm7.ld" "$LOG_DIR/"
cp "$ROOT_DIR/cmake/targets/stm32h755_cm4.ld" "$LOG_DIR/"
cp "$ROOT_DIR/tests/link/stm32h755/custom_cm7.ld" "$LOG_DIR/"
if command -v arm-none-eabi-size >/dev/null 2>&1; then
  arm-none-eabi-size "$CM7_ELF" >"$LOG_DIR/cm7-elf-size.txt" 2>&1 || true
  arm-none-eabi-size "$CLOCK_ELF" >"$LOG_DIR/cm7-clock-elf-size.txt" 2>&1 || true
  arm-none-eabi-size "$CM4_ELF" >"$LOG_DIR/cm4-elf-size.txt" 2>&1 || true
  arm-none-eabi-size "$CUSTOM_ELF" >"$LOG_DIR/custom-elf-size.txt" 2>&1 || true
fi
arm-none-eabi-nm -n "$CM7_ELF" >"$LOG_DIR/cm7-symbols.txt" 2>&1 || true
arm-none-eabi-nm -n "$CLOCK_ELF" >"$LOG_DIR/cm7-clock-symbols.txt" 2>&1 || true
arm-none-eabi-nm -n "$CM4_ELF" >"$LOG_DIR/cm4-symbols.txt" 2>&1 || true
arm-none-eabi-nm -n "$CUSTOM_ELF" >"$LOG_DIR/custom-symbols.txt" 2>&1 || true

safe_log_name() {
  local name="$1"
  name="${name//[^[:alnum:]._-]/_}"
  while [[ "$name" == *"__"* ]]; do name="${name//__/_}"; done
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
  printf '%-34s %s\n' "$name" "$status" | tee -a "$SUMMARY"
  if [[ "$status" == PASS ]]; then ((PASS_COUNT += 1)); else ((FAIL_COUNT += 1)); fi
}

run_gdb() {
  local elf="$1" port="$2" log="$3"
  shift 3
  mkdir -p "$(dirname "$log")"
  set +e
  timeout "${DEBUG_TIMEOUT}s" "$GDB_BIN" -q "$elf" -batch \
    -ex "target extended-remote :${port}" "$@" 2>&1 | tee "$log"
  local pipe_status=("${PIPESTATUS[@]}")
  local gdb_rc=${pipe_status[0]}
  local tee_rc=${pipe_status[1]}
  set -e
  (( gdb_rc == 0 && tee_rc == 0 )) && grep -q '^RESULT: PASS$' "$log"
}

check_layout() {
  local name="$1" log="$2"
  shift 2
  if "$ROOT_DIR/scripts/check_stm32h755_memory_layout.sh" "$@" 2>&1 | tee "$log"; then
    record "$name" PASS
  else
    record "$name" FAIL
    return 1
  fi
}

check_layout "STM32H755 CM7 memory layout" "$LOG_DIR/cm7_memory_layout.log" \
  --core cm7 "$CM7_ELF" "$CM7_MAP" || exit 1
check_layout "STM32H755 CM4 memory layout" "$LOG_DIR/cm4_memory_layout.log" \
  --core cm4 "$CM4_ELF" "$CM4_MAP" || exit 1
check_layout "Custom linker override" "$LOG_DIR/custom_memory_layout.log" \
  --core cm7 --flash-begin 0x08020000 --flash-end 0x08100000 \
  "$CUSTOM_ELF" "$CUSTOM_MAP" || exit 1

echo "Starting dual-core OpenOCD..."
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
    echo "OpenOCD exited before both GDB servers became ready" >&2
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

probe_core() {
  local label="$1" elf="$2" port="$3" expected_part="$4"
  local log="$LOG_DIR/$(safe_log_name "$label").log"
  if run_gdb "$elf" "$port" "$log" \
      -ex "set \$das_expected_part=$expected_part" \
      -x "$ROOT_DIR/scripts/gdb/stm32h755_probe.gdb"; then
    record "$label" PASS
  else
    record "$label" FAIL
    return 1
  fi
}

probe_core "CM7 OpenOCD probe" "$CM7_ELF" 3333 0xc27 || exit 1
probe_core "CM4 OpenOCD probe" "$CM4_ELF" 3334 0xc24 || exit 1

if run_gdb "$CLOCK_ELF" 3333 "$LOG_DIR/CM7_clock_HSI_PLL_400.log" \
    -x "$ROOT_DIR/scripts/gdb/stm32h755_clock_case.gdb"; then
  record "CM7 HSI/PLL 400MHz clock" PASS
else
  record "CM7 HSI/PLL 400MHz clock" FAIL
  exit 1
fi

bring_up_core() {
  local core="$1" elf="$2" port="$3"
  local prefix="${core}"
  if run_gdb "$elf" "$port" "$LOG_DIR/${core}_flash_probe.log" \
      -x "$ROOT_DIR/scripts/gdb/stm32h755_flash_probe.gdb"; then
    record "$prefix CMSIS/GPIO bring-up" PASS
  else
    record "$prefix CMSIS/GPIO bring-up" FAIL
    return 1
  fi

  if run_gdb "$elf" "$port" "$LOG_DIR/${core}_startup_reset.log" \
      -x "$ROOT_DIR/scripts/gdb/stm32h755_startup_probe.gdb"; then
    record "$prefix Cortex-M startup/reset" PASS
  else
    record "$prefix Cortex-M startup/reset" FAIL
    return 1
  fi
}

automated_gpio_case() {
  local core="$1" elf="$2" port="$3" name="$4" command="$5" expected_flags="$6"
  local label="$core $name"
  local log="$LOG_DIR/$(safe_log_name "$label").log"
  if run_gdb "$elf" "$port" "$log" \
      -ex "set \$das_command=$command" \
      -ex "set \$das_expected_flags=$expected_flags" \
      -x "$ROOT_DIR/scripts/gdb/stm32h755_gpio_case.gdb"; then
    record "$label" PASS
  else
    record "$label" FAIL
  fi
}

visual_case() {
  local core="$1" elf="$2" port="$3" name="$4" command="$5" expected_mask="$6" prompt="$7"
  local label="$core $name"
  local log="$LOG_DIR/$(safe_log_name "$label").log"
  local automated=FAIL visual=FAIL

  if run_gdb "$elf" "$port" "$log" \
      -ex "set \$das_command=$command" \
      -ex "set \$das_expected_mask=$expected_mask" \
      -x "$ROOT_DIR/scripts/gdb/stm32h755_led_case.gdb"; then
    automated=PASS
  fi

  if [[ "$automated" == PASS ]] && yes_no "$prompt"; then visual=PASS; fi

  if [[ "$automated" == PASS && "$visual" == PASS ]]; then
    record "$label" PASS
  else
    record "$label" FAIL
  fi
}

bring_up_core "CM7" "$CM7_ELF" 3333 || exit 1

wait_for_enter "CM7 pull tests: leave CN10 D3 / PE13 / pin 10 electrically DISCONNECTED. Remove any jumper or shield drive from that pin."
automated_gpio_case "CM7" "$CM7_ELF" 3333 "GPIO pull-up" 7 4
automated_gpio_case "CM7" "$CM7_ELF" 3333 "GPIO pull-down" 8 8

wait_for_enter "CM7 loopback tests: connect ONE jumper from CN10 D4 / PE14 / pin 8 (output) to CN10 D3 / PE13 / pin 10 (input). Do not connect either pin to 3V3, 5V, or GND."
automated_gpio_case "CM7" "$CM7_ELF" 3333 "GPIO loopback low/high" 6 3
automated_gpio_case "CM7" "$CM7_ELF" 3333 "GPIO open-drain" 9 48
automated_gpio_case "CM7" "$CM7_ELF" 3333 "GPIO EXTI rising/falling" 10 192

visual_case "CM7" "$CM7_ELF" 3333 "LED all off" 1 0 "Are green, yellow, and red user LEDs all OFF"
visual_case "CM7" "$CM7_ELF" 3333 "LED green only" 2 1 "Is only the GREEN user LED ON"
visual_case "CM7" "$CM7_ELF" 3333 "LED yellow only" 3 2 "Is only the YELLOW user LED ON"
visual_case "CM7" "$CM7_ELF" 3333 "LED red only" 4 4 "Is only the RED user LED ON"
visual_case "CM7" "$CM7_ELF" 3333 "LED all blink" 5 0 "Are all three user LEDs visibly BLINKING together"

run_gdb "$CM7_ELF" 3333 "$LOG_DIR/cm7_final_all_off.log" \
  -ex 'set $das_command=1' \
  -ex 'set $das_expected_mask=0' \
  -x "$ROOT_DIR/scripts/gdb/stm32h755_led_case.gdb" >/dev/null || true

bring_up_core "CM4" "$CM4_ELF" 3334 || exit 1

wait_for_enter "CM4 pull tests: DISCONNECT the D4-to-D3 jumper again. Leave CN10 D3 / PE13 / pin 10 electrically disconnected."
automated_gpio_case "CM4" "$CM4_ELF" 3334 "GPIO pull-up" 7 4
automated_gpio_case "CM4" "$CM4_ELF" 3334 "GPIO pull-down" 8 8

wait_for_enter "CM4 loopback tests: reconnect ONE jumper from CN10 D4 / PE14 / pin 8 to CN10 D3 / PE13 / pin 10. Do not connect either pin to 3V3, 5V, or GND."
automated_gpio_case "CM4" "$CM4_ELF" 3334 "GPIO loopback low/high" 6 3
automated_gpio_case "CM4" "$CM4_ELF" 3334 "GPIO open-drain" 9 48
automated_gpio_case "CM4" "$CM4_ELF" 3334 "GPIO EXTI rising/falling" 10 192

(( FAIL_COUNT == 0 )) || exit 1
