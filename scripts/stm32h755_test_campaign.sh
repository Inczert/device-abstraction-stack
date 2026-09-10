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
  --no-build              Reuse existing CM7/CM4 hardware, time, clock, button,
                          UART, SPI, I2C, DMA/cache, timer/PWM, and custom-link ELFs.
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
      STM32_CUBE_H7_DIR="$1"
      shift
      ;;
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

need() {
  command -v "$1" >/dev/null 2>&1 || { echo "Missing command: $1" >&2; exit 2; }
}
for command in cmake openocd timeout tee grep tar arm-none-eabi-nm; do need "$command"; done
if command -v gdb-multiarch >/dev/null 2>&1; then
  GDB_BIN=gdb-multiarch
elif command -v arm-none-eabi-gdb >/dev/null 2>&1; then
  GDB_BIN=arm-none-eabi-gdb
else
  echo "Install gdb-multiarch or arm-none-eabi-gdb" >&2
  exit 2
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
  echo "Persistent UART fixture: Arduino D1/TX/PB6 <-> Arduino D0/RX/PB7"
  echo "Persistent SPI fixture: Arduino D11/MOSI/PB5 <-> Arduino D12/MISO/PA6"
  echo "Persistent I2C SCL fixture: Arduino D15/PB8 <-> Zio D69/PF14 (CN9 pin 19)"
  echo "Persistent I2C SDA fixture: Arduino D14/PB9 <-> Zio D68/PF15 (CN9 pin 21)"
  echo "Switched GPIO/PWM fixture: CN10 D4/PE14 <-> CN10 D3/PE13"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$ROOT_DIR/cmake/targets/stm32h755_cm7.ld" 2>/dev/null || true
    sha256sum "$ROOT_DIR/cmake/targets/stm32h755_cm4.ld" 2>/dev/null || true
    sha256sum "$ROOT_DIR/tests/link/stm32h755/custom_cm7.ld" 2>/dev/null || true
    sha256sum "$ROOT_DIR/scripts/gdb/stm32h755_uart_case.gdb" 2>/dev/null || true
    sha256sum "$ROOT_DIR/scripts/gdb/stm32h755_spi_case.gdb" 2>/dev/null || true
    sha256sum "$ROOT_DIR/scripts/gdb/stm32h755_i2c_case.gdb" 2>/dev/null || true
    sha256sum "$ROOT_DIR/scripts/gdb/stm32h755_dma_case.gdb" 2>/dev/null || true
    sha256sum "$ROOT_DIR/scripts/gdb/stm32h755_timer_case.gdb" 2>/dev/null || true
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
  (( CM7_BUILD_RC == 0 )) || exit "$CM7_BUILD_RC"

  set +e
  "$ROOT_DIR/scripts/build_stm32h755.sh" \
    --stm32h7-root "$STM32_CUBE_H7_DIR" \
    --build-dir "$CM4_BUILD_DIR" \
    --core cm4 2>&1 | tee "$CM4_BUILD_LOG"
  CM4_BUILD_RC=${PIPESTATUS[0]}
  set -e
  (( CM4_BUILD_RC == 0 )) || exit "$CM4_BUILD_RC"

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
  (( CUSTOM_BUILD_RC == 0 )) || exit "$CUSTOM_BUILD_RC"
else
  echo "Build skipped; reusing existing campaign ELFs/maps." | tee "$BUILD_LOG"
fi

CM7_ELF="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_hw_test.elf"
CM7_MAP="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_hw_test.map"
CM7_TIME_ELF="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_time_test.elf"
CM7_TIME_MAP="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_time_test.map"
CM7_UART_ELF="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_uart_test.elf"
CM7_UART_MAP="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_uart_test.map"
CM7_SPI_ELF="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_spi_test.elf"
CM7_SPI_MAP="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_spi_test.map"
CM7_I2C_ELF="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_i2c_test.elf"
CM7_I2C_MAP="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_i2c_test.map"
CM7_DMA_ELF="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_dma_test.elf"
CM7_DMA_MAP="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_dma_test.map"
CM7_TIMER_ELF="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_timer_test.elf"
CM7_TIMER_MAP="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_timer_test.map"
CLOCK_ELF="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_clock_test.elf"
CLOCK_MAP="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_clock_test.map"
BUTTON_ELF="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_button_test.elf"
BUTTON_MAP="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_button_test.map"
CM4_ELF="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_hw_test.elf"
CM4_MAP="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_hw_test.map"
CM4_TIME_ELF="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_time_test.elf"
CM4_TIME_MAP="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_time_test.map"
CM4_UART_ELF="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_uart_test.elf"
CM4_UART_MAP="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_uart_test.map"
CM4_SPI_ELF="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_spi_test.elf"
CM4_SPI_MAP="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_spi_test.map"
CM4_I2C_ELF="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_i2c_test.elf"
CM4_I2C_MAP="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_i2c_test.map"
CM4_DMA_ELF="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_dma_test.elf"
CM4_DMA_MAP="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_dma_test.map"
CM4_TIMER_ELF="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_timer_test.elf"
CM4_TIMER_MAP="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_timer_test.map"
CUSTOM_ELF="$CUSTOM_BUILD_DIR/tests/link/stm32h755/das_stm32h755_link_test.elf"
CUSTOM_MAP="$CUSTOM_BUILD_DIR/tests/link/stm32h755/das_stm32h755_link_test.map"

ARTIFACTS=(
  "$CM7_ELF" "$CM7_MAP" "$CM7_TIME_ELF" "$CM7_TIME_MAP"
  "$CM7_UART_ELF" "$CM7_UART_MAP" "$CM7_SPI_ELF" "$CM7_SPI_MAP"
  "$CM7_I2C_ELF" "$CM7_I2C_MAP" "$CM7_DMA_ELF" "$CM7_DMA_MAP"
  "$CM7_TIMER_ELF" "$CM7_TIMER_MAP"
  "$CLOCK_ELF" "$CLOCK_MAP" "$BUTTON_ELF" "$BUTTON_MAP"
  "$CM4_ELF" "$CM4_MAP" "$CM4_TIME_ELF" "$CM4_TIME_MAP"
  "$CM4_UART_ELF" "$CM4_UART_MAP" "$CM4_SPI_ELF" "$CM4_SPI_MAP"
  "$CM4_I2C_ELF" "$CM4_I2C_MAP" "$CM4_DMA_ELF" "$CM4_DMA_MAP"
  "$CM4_TIMER_ELF" "$CM4_TIMER_MAP"
  "$CUSTOM_ELF" "$CUSTOM_MAP"
)
for path in "${ARTIFACTS[@]}"; do
  [[ -s "$path" ]] || { echo "Expected campaign artifact not found: $path" >&2; exit 1; }
done

copy_pair() {
  local elf="$1" map="$2" stem="$3"
  cp "$elf" "$LOG_DIR/${stem}.elf"
  cp "$map" "$LOG_DIR/${stem}.map"
  if command -v arm-none-eabi-size >/dev/null 2>&1; then
    arm-none-eabi-size "$elf" >"$LOG_DIR/${stem}-size.txt" 2>&1 || true
  fi
  arm-none-eabi-nm -n "$elf" >"$LOG_DIR/${stem}-symbols.txt" 2>&1 || true
}

copy_pair "$CM7_ELF" "$CM7_MAP" das_stm32h755_cm7_hw_test
copy_pair "$CM7_TIME_ELF" "$CM7_TIME_MAP" das_stm32h755_cm7_time_test
copy_pair "$CM7_UART_ELF" "$CM7_UART_MAP" das_stm32h755_cm7_uart_test
copy_pair "$CM7_SPI_ELF" "$CM7_SPI_MAP" das_stm32h755_cm7_spi_test
copy_pair "$CM7_I2C_ELF" "$CM7_I2C_MAP" das_stm32h755_cm7_i2c_test
copy_pair "$CM7_DMA_ELF" "$CM7_DMA_MAP" das_stm32h755_cm7_dma_test
copy_pair "$CM7_TIMER_ELF" "$CM7_TIMER_MAP" das_stm32h755_cm7_timer_test
copy_pair "$CLOCK_ELF" "$CLOCK_MAP" das_stm32h755_cm7_clock_test
copy_pair "$BUTTON_ELF" "$BUTTON_MAP" das_stm32h755_cm7_button_test
copy_pair "$CM4_ELF" "$CM4_MAP" das_stm32h755_cm4_hw_test
copy_pair "$CM4_TIME_ELF" "$CM4_TIME_MAP" das_stm32h755_cm4_time_test
copy_pair "$CM4_UART_ELF" "$CM4_UART_MAP" das_stm32h755_cm4_uart_test
copy_pair "$CM4_SPI_ELF" "$CM4_SPI_MAP" das_stm32h755_cm4_spi_test
copy_pair "$CM4_I2C_ELF" "$CM4_I2C_MAP" das_stm32h755_cm4_i2c_test
copy_pair "$CM4_DMA_ELF" "$CM4_DMA_MAP" das_stm32h755_cm4_dma_test
copy_pair "$CM4_TIMER_ELF" "$CM4_TIMER_MAP" das_stm32h755_cm4_timer_test
copy_pair "$CUSTOM_ELF" "$CUSTOM_MAP" das_stm32h755_custom_link_test
cp "$ROOT_DIR/cmake/targets/stm32h755_cm7.ld" "$LOG_DIR/"
cp "$ROOT_DIR/cmake/targets/stm32h755_cm4.ld" "$LOG_DIR/"
cp "$ROOT_DIR/tests/link/stm32h755/custom_cm7.ld" "$LOG_DIR/"

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

initial_hardware_setup() {
  cat <<'SETUP'

=== Initial hardware setup ===
1. Connect the NUCLEO-H755ZI-Q through the ST-LINK USB connection.
2. Connect jumper A and LEAVE IT CONNECTED for the entire campaign:
     Arduino D1 / TX / PB6  <->  Arduino D0 / RX / PB7
3. Connect jumper B and LEAVE IT CONNECTED for the entire campaign:
     Arduino D11 / MOSI / PB5  <->  Arduino D12 / MISO / PA6
4. Connect jumper C and LEAVE IT CONNECTED for the entire campaign:
     Arduino D15 / PB8 / I2C_A_SCL  <->  Zio D69 / PF14 / I2C_B_SCL
                                           CN9 pin 19
5. Connect jumper D and LEAVE IT CONNECTED for the entire campaign:
     Arduino D14 / PB9 / I2C_A_SDA  <->  Zio D68 / PF15 / I2C_B_SDA
                                           CN9 pin 21
6. Leave CN10 D3 / PE13 and CN10 D4 / PE14 DISCONNECTED for now.
7. Keep jumper E ready. The campaign will ask ONCE when it is time to connect
   D4 <-> D3; after that, leave it connected for the rest of the run.
8. Leave Arduino D13 / SCK / PA5 and D10 / CS / PD14 otherwise unconnected.
9. Leave the blue B1 USER button released.

Never connect the loopback signal pins to 3V3, 5V, or GND.
SETUP
  read -r -p "Press ENTER when the initial setup is complete... "
}

record() {
  local name="$1" status="$2"
  printf '%-34s %s\n' "$name" "$status" | tee -a "$SUMMARY"
  if [[ "$status" == PASS ]]; then
    ((PASS_COUNT += 1))
  else
    ((FAIL_COUNT += 1))
  fi
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

run_simple_case() {
  local label="$1" elf="$2" port="$3" gdb_script="$4"
  local log="$LOG_DIR/$(safe_log_name "$label").log"
  if run_gdb "$elf" "$port" "$log" -x "$gdb_script"; then
    record "$label" PASS
  else
    record "$label" FAIL
    return 1
  fi
}

run_dma_case() {
  local label="$1" elf="$2" port="$3" expected_cache="$4" expected_hz="$5"
  local log="$LOG_DIR/$(safe_log_name "$label").log"
  if run_gdb "$elf" "$port" "$log" \
      -ex "set \$das_expected_cache=$expected_cache" \
      -ex "set \$das_expected_core_hz=$expected_hz" \
      -x "$ROOT_DIR/scripts/gdb/stm32h755_dma_case.gdb"; then
    record "$label" PASS
  else
    record "$label" FAIL
    return 1
  fi
}

run_button_case() {
  local label="CM7 user button input/EXTI"
  if ! run_gdb "$BUTTON_ELF" 3333 "$LOG_DIR/CM7_button_setup.log" \
      -x "$ROOT_DIR/scripts/gdb/stm32h755_button_setup.gdb"; then
    record "$label" FAIL
    return 1
  fi

  wait_for_enter "User-button test: press and HOLD the blue B1 USER button, then press ENTER while still holding it."
  if ! run_gdb "$BUTTON_ELF" 3333 "$LOG_DIR/CM7_button_pressed.log" \
      -ex 'set $das_expected_pressed=1' \
      -x "$ROOT_DIR/scripts/gdb/stm32h755_button_state.gdb"; then
    record "$label" FAIL
    return 1
  fi

  wait_for_enter "User-button test: RELEASE the blue B1 USER button, then press ENTER."
  if run_gdb "$BUTTON_ELF" 3333 "$LOG_DIR/CM7_button_released.log" \
      -ex 'set $das_expected_pressed=0' \
      -x "$ROOT_DIR/scripts/gdb/stm32h755_button_state.gdb"; then
    record "$label" PASS
  else
    record "$label" FAIL
    return 1
  fi
}

bring_up_core() {
  local core="$1" elf="$2" port="$3"
  if run_gdb "$elf" "$port" "$LOG_DIR/${core}_flash_probe.log" \
      -x "$ROOT_DIR/scripts/gdb/stm32h755_flash_probe.gdb"; then
    record "$core CMSIS/GPIO bring-up" PASS
  else
    record "$core CMSIS/GPIO bring-up" FAIL
    return 1
  fi

  if run_gdb "$elf" "$port" "$LOG_DIR/${core}_startup_reset.log" \
      -x "$ROOT_DIR/scripts/gdb/stm32h755_startup_probe.gdb"; then
    record "$core Cortex-M startup/reset" PASS
  else
    record "$core Cortex-M startup/reset" FAIL
    return 1
  fi
}

rearm_core_image() {
  local core="$1" elf="$2" port="$3"
  local log="$LOG_DIR/${core}_fixture_rearm.log"
  if run_gdb "$elf" "$port" "$log" \
      -x "$ROOT_DIR/scripts/gdb/stm32h755_flash_probe.gdb"; then
    return 0
  fi
  echo "Failed to re-arm $core hardware image after fixture transition" >&2
  return 1
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

# Host/static qualification. These deliberately do not require the board.
check_layout "STM32H755 CM7 memory layout" "$LOG_DIR/cm7_memory_layout.log" \
  --core cm7 "$CM7_ELF" "$CM7_MAP" || exit 1
check_layout "STM32H755 CM4 memory layout" "$LOG_DIR/cm4_memory_layout.log" \
  --core cm4 "$CM4_ELF" "$CM4_MAP" || exit 1
check_layout "Custom linker override" "$LOG_DIR/custom_memory_layout.log" \
  --core cm7 --flash-begin 0x08020000 --flash-end 0x08100000 \
  "$CUSTOM_ELF" "$CUSTOM_MAP" || exit 1

# One physical setup prompt covers every persistent serial fixture and leaves
# D3/D4 free until the single later transition.
initial_hardware_setup

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

probe_core "CM7 OpenOCD probe" "$CM7_ELF" 3333 0xc27 || exit 1
probe_core "CM4 OpenOCD probe" "$CM4_ELF" 3334 0xc24 || exit 1
run_simple_case "CM7 monotonic timebase" "$CM7_TIME_ELF" 3333 \
  "$ROOT_DIR/scripts/gdb/stm32h755_time_case.gdb" || exit 1
run_simple_case "CM4 monotonic timebase" "$CM4_TIME_ELF" 3334 \
  "$ROOT_DIR/scripts/gdb/stm32h755_time_case.gdb" || exit 1
run_simple_case "CM7 HSI/PLL 400MHz clock" "$CLOCK_ELF" 3333 \
  "$ROOT_DIR/scripts/gdb/stm32h755_clock_case.gdb" || exit 1
run_button_case || exit 1

# Persistent serial fixtures were installed during initial setup and stay connected.
run_simple_case "CM7 UART loopback" "$CM7_UART_ELF" 3333 \
  "$ROOT_DIR/scripts/gdb/stm32h755_uart_case.gdb" || exit 1
run_simple_case "CM4 UART loopback" "$CM4_UART_ELF" 3334 \
  "$ROOT_DIR/scripts/gdb/stm32h755_uart_case.gdb" || exit 1
run_simple_case "CM7 SPI loopback" "$CM7_SPI_ELF" 3333 \
  "$ROOT_DIR/scripts/gdb/stm32h755_spi_case.gdb" || exit 1
run_simple_case "CM4 SPI loopback" "$CM4_SPI_ELF" 3334 \
  "$ROOT_DIR/scripts/gdb/stm32h755_spi_case.gdb" || exit 1

# DMA/cache reuses the same persistent SPI physical loopback and the already-running
# dual-core OpenOCD session; no additional fixture transition is required.
run_dma_case "CM7 DMA/cache" "$CM7_DMA_ELF" 3333 1 400000000 || exit 1
run_dma_case "CM4 DMA/cache" "$CM4_DMA_ELF" 3334 0 64000000 || exit 1

run_simple_case "CM7 I2C controller/target" "$CM7_I2C_ELF" 3333 \
  "$ROOT_DIR/scripts/gdb/stm32h755_i2c_case.gdb" || exit 1
run_simple_case "CM4 I2C controller/target" "$CM4_I2C_ELF" 3334 \
  "$ROOT_DIR/scripts/gdb/stm32h755_i2c_case.gdb" || exit 1

# Qualify all tests requiring D3 to be electrically free before touching D4/D3.
bring_up_core "CM7" "$CM7_ELF" 3333 || exit 1
automated_gpio_case "CM7" "$CM7_ELF" 3333 "GPIO pull-up" 7 4
automated_gpio_case "CM7" "$CM7_ELF" 3333 "GPIO pull-down" 8 8

bring_up_core "CM4" "$CM4_ELF" 3334 || exit 1
automated_gpio_case "CM4" "$CM4_ELF" 3334 "GPIO pull-up" 7 4
automated_gpio_case "CM4" "$CM4_ELF" 3334 "GPIO pull-down" 8 8

# Single fixture transition. D4/D3 remains connected through every remaining
# GPIO and timer/PWM case; all serial fixtures remain connected too.
wait_for_enter "Fixture transition: connect jumper E from CN10 D4 / PE14 to CN10 D3 / PE13. Leave D4-D3 and all UART/SPI/I2C fixture jumpers connected for the rest of the campaign."

# CM4 is already running its hardware image from the free-D3 phase.
automated_gpio_case "CM4" "$CM4_ELF" 3334 "GPIO loopback low/high" 6 3
automated_gpio_case "CM4" "$CM4_ELF" 3334 "GPIO open-drain" 9 48
automated_gpio_case "CM4" "$CM4_ELF" 3334 "GPIO EXTI rising/falling" 10 192

# Re-arm CM7 once after the CM4 phase; this is fixture choreography, not a new
# acceptance point. The CM7 bring-up/startup acceptance points were recorded above.
rearm_core_image "CM7" "$CM7_ELF" 3333 || exit 1
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

# Timer/PWM reuses the already-installed D4/D3 fixture. No extra wiring prompt.
run_simple_case "CM7 timer/PWM" "$CM7_TIMER_ELF" 3333 \
  "$ROOT_DIR/scripts/gdb/stm32h755_timer_case.gdb" || exit 1
run_simple_case "CM4 timer/PWM" "$CM4_TIMER_ELF" 3334 \
  "$ROOT_DIR/scripts/gdb/stm32h755_timer_case.gdb" || exit 1

(( FAIL_COUNT == 0 )) || exit 1