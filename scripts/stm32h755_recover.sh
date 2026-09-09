#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OPENOCD_SCRIPTS="${OPENOCD_SCRIPTS:-/usr/share/openocd/scripts}"

command -v openocd >/dev/null 2>&1 || { echo "Missing command: openocd" >&2; exit 2; }

echo "Mass-erasing both STM32H755 flash banks. This destroys the current firmware."
openocd -s "$OPENOCD_SCRIPTS" \
  -f "$ROOT_DIR/scripts/openocd_h755.cfg" \
  -c "init; reset halt; stm32h7x mass_erase 0; stm32h7x mass_erase 1; reset halt; shutdown"
