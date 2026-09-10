#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STM32_CUBE_H7_DIR="${STM32_CUBE_H7_DIR:-}"
BUILD_DIR="${DAS_STM32_BUILD_DIR:-$ROOT_DIR/build/stm32h755}"
CORE="cm7"
CLEAN=0

usage() {
  cat <<'USAGE'
Usage:
  scripts/build_stm32h755.sh /path/to/STM32CubeH7 [options]
  scripts/build_stm32h755.sh --stm32h7-root /path/to/STM32CubeH7 [options]

Options:
  --stm32h7-root DIR  STM32CubeH7 checkout root.
  --build-dir DIR     Build directory.
  --core cm7|cm4      STM32H755 core to build (default: cm7).
  --clean             Remove the build directory first.
  -h, --help          Show help.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --stm32h7-root) STM32_CUBE_H7_DIR="$2"; shift 2 ;;
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    --core) CORE="$2"; shift 2 ;;
    --clean) CLEAN=1; shift ;;
    -h|--help) usage; exit 0 ;;
    --*) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    *)
      [[ -z "$STM32_CUBE_H7_DIR" ]] || { echo "Unexpected argument: $1" >&2; exit 2; }
      STM32_CUBE_H7_DIR="$1"; shift ;;
  esac
done

[[ "$CORE" == "cm7" || "$CORE" == "cm4" ]] || {
  echo "--core must be cm7 or cm4" >&2
  exit 2
}

for command in cmake arm-none-eabi-gcc arm-none-eabi-size; do
  command -v "$command" >/dev/null 2>&1 || { echo "Missing command: $command" >&2; exit 2; }
done
[[ -n "$STM32_CUBE_H7_DIR" ]] || { usage >&2; exit 2; }
STM32_CUBE_H7_DIR="$(cd "$STM32_CUBE_H7_DIR" 2>/dev/null && pwd)" || {
  echo "Invalid STM32CubeH7 root: $STM32_CUBE_H7_DIR" >&2
  exit 2
}

(( CLEAN == 0 )) || rm -rf -- "$BUILD_DIR"

# Hardware-test ELFs deliberately own explicit test vector tables so they can
# bind test-only ISRs. Disable the normal application default table in this
# build configuration to keep exactly one .isr_vector contribution per ELF.
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/cmake/toolchains/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDAS_DEVICE=nucleo_h755zi_q \
  -DDAS_CORE="$CORE" \
  -DDAS_USE_DEFAULT_VECTOR_TABLE=OFF \
  -DSTM32_CUBE_H7_DIR="$STM32_CUBE_H7_DIR" \
  -DDAS_BUILD_HARDWARE_TESTS=ON
cmake --build "$BUILD_DIR" --target das_stm32h755_hw_test --parallel

ELF="$BUILD_DIR/tests/hardware/stm32h755/das_stm32h755_hw_test.elf"
[[ -s "$ELF" ]] || { echo "Expected ELF not found: $ELF" >&2; exit 1; }
arm-none-eabi-size "$ELF"
echo "Built $CORE: $ELF"
