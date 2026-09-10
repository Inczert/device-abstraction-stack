#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PREFIX="${1:-}"
CORE="${2:-}"
BUILD_ROOT="${3:-$ROOT_DIR/build/ci-examples-$CORE}"

[[ -n "$PREFIX" && -d "$PREFIX" ]] || {
  echo "Usage: $0 /path/to/das-install cm7|cm4 [build-dir]" >&2
  exit 2
}
[[ "$CORE" == "cm7" || "$CORE" == "cm4" ]] || {
  echo "Core must be cm7 or cm4" >&2
  exit 2
}

for command in cmake arm-none-eabi-nm arm-none-eabi-objdump; do
  command -v "$command" >/dev/null 2>&1 || {
    echo "Missing command: $command" >&2
    exit 2
  }
done

PREFIX="$(cd "$PREFIX" && pwd)"
EXPECTED_DAS_DIR="$PREFIX/lib/cmake/DAS"
TOOLCHAIN="$ROOT_DIR/cmake/toolchains/arm-none-eabi.cmake"
EXAMPLES=(led_blink time_periodic uart_console clock_uart)

rm -rf "$BUILD_ROOT"
mkdir -p "$BUILD_ROOT"

for example in "${EXAMPLES[@]}"; do
  source_dir="$ROOT_DIR/examples/$example"
  build_dir="$BUILD_ROOT/$example"

  [[ -f "$source_dir/CMakeLists.txt" ]] || {
    echo "Example is not a standalone CMake consumer: $example" >&2
    exit 1
  }
  grep -q 'find_package(DAS' "$source_dir/CMakeLists.txt" || {
    echo "Example does not consume installed DAS with find_package(): $example" >&2
    exit 1
  }

  cmake -S "$source_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DDAS_CORE="$CORE" \
    -DCMAKE_PREFIX_PATH="$PREFIX"
  cmake --build "$build_dir" --parallel

  resolved="$(sed -n 's/^DAS_DIR:PATH=//p' "$build_dir/CMakeCache.txt" | tail -n 1)"
  [[ -n "$resolved" ]] || {
    echo "DAS_DIR missing from CMake cache for $example" >&2
    exit 1
  }
  [[ "$(readlink -f "$resolved")" == "$(readlink -f "$EXPECTED_DAS_DIR")" ]] || {
    echo "$example resolved unexpected DAS package: $resolved" >&2
    exit 1
  }

  mapfile -t elfs < <(find "$build_dir" -maxdepth 1 -type f -name '*.elf' -print)
  [[ ${#elfs[@]} -eq 1 && -s "${elfs[0]}" ]] || {
    echo "Expected exactly one ELF for $example" >&2
    exit 1
  }

done

LED_ELF="$BUILD_ROOT/led_blink/das_led_blink.elf"
arm-none-eabi-nm "$LED_ELF" | grep -Eq '[[:space:]][A-Za-z][[:space:]]g_das_vector_table$' || {
  echo "Default DAS vector table missing from LED consumer" >&2
  exit 1
}
arm-none-eabi-objdump -h "$LED_ELF" | grep -q '\.isr_vector' || {
  echo "LED consumer has no .isr_vector section" >&2
  exit 1
}

echo "Installed DAS examples built successfully for $CORE from $PREFIX"
