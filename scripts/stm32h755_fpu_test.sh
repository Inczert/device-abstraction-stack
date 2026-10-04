#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STM32_CUBE_H7_DIR="${STM32_CUBE_H7_DIR:-}"
BUILD_ROOT="${DAS_FPU_BUILD_DIR:-$ROOT_DIR/build/stm32h755-fpu}"
CM7_BUILD_DIR=""
CM4_BUILD_DIR=""
LOG_DIR="${DAS_FPU_TEST_LOG_DIR:-}"
OPENOCD_SCRIPTS="${OPENOCD_SCRIPTS:-/usr/share/openocd/scripts}"
DEBUG_TIMEOUT=30
NO_BUILD=0
OPENOCD_PID=""
GDB_BIN=""

usage() {
  cat <<'USAGE'
Usage:
  scripts/stm32h755_fpu_test.sh /path/to/STM32CubeH7 [options]
  scripts/stm32h755_fpu_test.sh --stm32h7-root /path/to/STM32CubeH7 [options]

Options:
  --stm32h7-root DIR      STM32CubeH7 checkout root.
  --build-root DIR        Standalone build root (default: build/stm32h755-fpu).
  --cm7-build-dir DIR     Existing/desired CM7 build directory.
  --cm4-build-dir DIR     Existing/desired CM4 build directory.
  --log-dir DIR           Evidence directory.
  --openocd-scripts DIR   OpenOCD scripts directory.
  --debug-timeout SEC     GDB timeout per core (default: 30).
  --no-build              Reuse existing CM7/CM4 hardware-test builds.
  -h, --help              Show help.

No external signal wiring is required. Connect the board through ST-LINK USB.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --stm32h7-root) STM32_CUBE_H7_DIR="$2"; shift 2 ;;
    --build-root) BUILD_ROOT="$2"; shift 2 ;;
    --cm7-build-dir) CM7_BUILD_DIR="$2"; shift 2 ;;
    --cm4-build-dir) CM4_BUILD_DIR="$2"; shift 2 ;;
    --log-dir) LOG_DIR="$2"; shift 2 ;;
    --openocd-scripts) OPENOCD_SCRIPTS="$2"; shift 2 ;;
    --debug-timeout) DEBUG_TIMEOUT="$2"; shift 2 ;;
    --no-build) NO_BUILD=1; shift ;;
    -h|--help) usage; exit 0 ;;
    --*) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    *)
      [[ -z "$STM32_CUBE_H7_DIR" ]] || {
        echo "Unexpected argument: $1" >&2
        exit 2
      }
      STM32_CUBE_H7_DIR="$1"
      shift
      ;;
  esac
done

[[ "$DEBUG_TIMEOUT" =~ ^[0-9]+$ ]] && (( DEBUG_TIMEOUT > 0 )) || {
  echo "--debug-timeout must be a positive integer" >&2
  exit 2
}

CM7_BUILD_DIR="${CM7_BUILD_DIR:-$BUILD_ROOT/cm7}"
CM4_BUILD_DIR="${CM4_BUILD_DIR:-$BUILD_ROOT/cm4}"
LOG_DIR="${LOG_DIR:-$BUILD_ROOT/logs}"

need() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Missing command: $1" >&2
    exit 2
  }
}

for command in openocd timeout tee grep arm-none-eabi-objdump; do
  need "$command"
done
if command -v gdb-multiarch >/dev/null 2>&1; then
  GDB_BIN=gdb-multiarch
elif command -v arm-none-eabi-gdb >/dev/null 2>&1; then
  GDB_BIN=arm-none-eabi-gdb
else
  echo "Install gdb-multiarch or arm-none-eabi-gdb" >&2
  exit 2
fi

[[ -n "$STM32_CUBE_H7_DIR" ]] || { usage >&2; exit 2; }
STM32_CUBE_H7_DIR="$(cd "$STM32_CUBE_H7_DIR" 2>/dev/null && pwd)" || {
  echo "Invalid STM32CubeH7 root: $STM32_CUBE_H7_DIR" >&2
  exit 2
}

cleanup() {
  local rc=$?
  trap - EXIT INT TERM
  if [[ -n "${OPENOCD_PID:-}" ]] && kill -0 "$OPENOCD_PID" >/dev/null 2>&1; then
    kill "$OPENOCD_PID" >/dev/null 2>&1 || true
    wait "$OPENOCD_PID" >/dev/null 2>&1 || true
  fi
  exit "$rc"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

mkdir -p "$LOG_DIR"
rm -f \
  "$LOG_DIR"/cm7.gdb.log \
  "$LOG_DIR"/cm4.gdb.log \
  "$LOG_DIR"/cm7.disassembly.txt \
  "$LOG_DIR"/cm4.disassembly.txt \
  "$LOG_DIR"/openocd.log \
  "$LOG_DIR"/metadata.txt

{
  echo "DAS STM32H755 hard-float startup qualification"
  echo "UTC start: $(date -u +'%Y-%m-%dT%H:%M:%SZ')"
  echo "Repository: $ROOT_DIR"
  if command -v git >/dev/null 2>&1; then
    echo "DAS commit: $(git -C "$ROOT_DIR" rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "STM32CubeH7 commit: $(git -C "$STM32_CUBE_H7_DIR" rev-parse HEAD 2>/dev/null || echo unknown)"
  fi
  echo "CM7 build: $CM7_BUILD_DIR"
  echo "CM4 build: $CM4_BUILD_DIR"
  echo "GDB: $GDB_BIN"
  arm-none-eabi-objdump --version 2>/dev/null | head -n 1 || true
  "$GDB_BIN" --version 2>/dev/null | head -n 1 || true
  openocd --version 2>&1 | head -n 1 || true
} >"$LOG_DIR/metadata.txt"

if (( NO_BUILD == 0 )); then
  "$ROOT_DIR/scripts/build_stm32h755.sh" \
    --stm32h7-root "$STM32_CUBE_H7_DIR" \
    --build-dir "$CM7_BUILD_DIR" \
    --core cm7

  "$ROOT_DIR/scripts/build_stm32h755.sh" \
    --stm32h7-root "$STM32_CUBE_H7_DIR" \
    --build-dir "$CM4_BUILD_DIR" \
    --core cm4
fi

CM7_ELF="$CM7_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_fpu_test.elf"
CM4_ELF="$CM4_BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_fpu_test.elf"

for elf in "$CM7_ELF" "$CM4_ELF"; do
  [[ -s "$elf" ]] || {
    echo "Expected FPU qualification ELF not found: $elf" >&2
    exit 1
  }
done

verify_vfp_image() {
  local core="$1"
  local elf="$2"
  local disassembly="$3"

  arm-none-eabi-objdump -d "$elf" >"$disassembly"
  if ! grep -Eq '[[:space:]]v(mul|add|mla|fma)\.f32[[:space:]]' "$disassembly"; then
    echo "$core image does not contain a qualifying VFP single-precision arithmetic instruction" >&2
    echo "The test would not prove hardware floating-point execution." >&2
    return 1
  fi
  echo "$core VFP instruction presence: PASS"
}

verify_vfp_image CM7 "$CM7_ELF" "$LOG_DIR/cm7.disassembly.txt"
verify_vfp_image CM4 "$CM4_ELF" "$LOG_DIR/cm4.disassembly.txt"

echo "Starting dual-core OpenOCD..."
openocd -s "$OPENOCD_SCRIPTS" \
  -f "$ROOT_DIR/scripts/openocd_h755_dual_core.cfg" \
  -c "init; reset halt" >"$LOG_DIR/openocd.log" 2>&1 &
OPENOCD_PID=$!

for ((attempt = 0; attempt < 150; ++attempt)); do
  if grep -q "Listening on port 3333 for gdb connections" "$LOG_DIR/openocd.log" 2>/dev/null &&
     grep -q "Listening on port 3334 for gdb connections" "$LOG_DIR/openocd.log" 2>/dev/null; then
    break
  fi
  if ! kill -0 "$OPENOCD_PID" >/dev/null 2>&1; then
    cat "$LOG_DIR/openocd.log" >&2
    echo "OpenOCD exited before both GDB servers became ready" >&2
    exit 1
  fi
  sleep 0.1
done

if ! grep -q "Listening on port 3333 for gdb connections" "$LOG_DIR/openocd.log" ||
   ! grep -q "Listening on port 3334 for gdb connections" "$LOG_DIR/openocd.log"; then
  cat "$LOG_DIR/openocd.log" >&2
  echo "Dual-core OpenOCD GDB server timeout" >&2
  exit 1
fi

run_core() {
  local core="$1"
  local elf="$2"
  local port="$3"
  local log="$4"

  set +e
  timeout "${DEBUG_TIMEOUT}s" "$GDB_BIN" -q "$elf" -batch \
    -ex "target extended-remote :$port" \
    -x "$ROOT_DIR/scripts/gdb/stm32h755_fpu_case.gdb" 2>&1 | tee "$log"
  local pipe_status=("${PIPESTATUS[@]}")
  local gdb_rc=${pipe_status[0]}
  local tee_rc=${pipe_status[1]}
  set -e

  if (( gdb_rc == 0 && tee_rc == 0 )) && grep -q '^RESULT: PASS$' "$log"; then
    echo "$core CP10/CP11 + hardware VFP execution: PASS"
    return 0
  fi

  echo "$core CP10/CP11 + hardware VFP execution: FAIL" >&2
  return 1
}

run_core CM7 "$CM7_ELF" 3333 "$LOG_DIR/cm7.gdb.log"
run_core CM4 "$CM4_ELF" 3334 "$LOG_DIR/cm4.gdb.log"

cat <<EOF

============================================================
STM32H755 HARD-FLOAT STARTUP QUALIFICATION: PASS
============================================================
CM7 CP10/CP11 + real VFP arithmetic: PASS
CM4 CP10/CP11 + real VFP arithmetic: PASS
Evidence: $LOG_DIR
EOF
