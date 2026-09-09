#!/usr/bin/env bash
set -Eeuo pipefail

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <firmware.elf> <firmware.map>" >&2
    exit 2
fi

ELF="$1"
MAP_FILE="$2"

for command in arm-none-eabi-nm grep awk; do
    command -v "$command" >/dev/null 2>&1 || {
        echo "Missing command: $command" >&2
        exit 2
    }
done

[[ -s "$ELF" ]] || { echo "ELF not found or empty: $ELF" >&2; exit 1; }
[[ -s "$MAP_FILE" ]] || { echo "Linker map not found or empty: $MAP_FILE" >&2; exit 1; }

NM_OUTPUT="$(arm-none-eabi-nm -n "$ELF")"

symbol_hex() {
    local symbol="$1"
    local value
    value="$(printf '%s\n' "$NM_OUTPUT" | awk -v wanted="$symbol" '$3 == wanted { print $1; exit }')"
    [[ -n "$value" ]] || {
        echo "Missing ELF symbol: $symbol" >&2
        return 1
    }
    printf '0x%s' "$value"
}

in_range() {
    local value=$(( $1 ))
    local begin=$(( $2 ))
    local end=$(( $3 ))
    (( value >= begin && value < end ))
}

required_symbols=(
    __vector_table_start__
    __vector_table_end__
    __data_load__
    __data_start__
    __data_end__
    __bss_start__
    __bss_end__
    __StackLimit
    __StackTop
    __HeapBase
    __HeapLimit
)

for symbol in "${required_symbols[@]}"; do
    grep -q -- "$symbol" "$MAP_FILE" || {
        echo "Linker map does not contain required symbol: $symbol" >&2
        exit 1
    }
done

vector_start="$(symbol_hex __vector_table_start__)"
vector_end="$(symbol_hex __vector_table_end__)"
data_load="$(symbol_hex __data_load__)"
data_start="$(symbol_hex __data_start__)"
data_end="$(symbol_hex __data_end__)"
bss_start="$(symbol_hex __bss_start__)"
bss_end="$(symbol_hex __bss_end__)"
stack_limit="$(symbol_hex __StackLimit)"
stack_top="$(symbol_hex __StackTop)"
heap_base="$(symbol_hex __HeapBase)"
heap_limit="$(symbol_hex __HeapLimit)"

FLASH_BEGIN=0x08000000
FLASH_END=0x08200000
AXISRAM_BEGIN=0x24000000
AXISRAM_END=0x24080000

[[ $((vector_start)) -eq $((FLASH_BEGIN)) ]] || {
    echo "Unexpected vector-table address: $vector_start (expected 0x08000000)" >&2
    exit 1
}

in_range "$vector_end" "$FLASH_BEGIN" "$FLASH_END" || {
    echo "Vector table ends outside STM32H755 flash: $vector_end" >&2
    exit 1
}
in_range "$data_load" "$FLASH_BEGIN" "$FLASH_END" || {
    echo ".data load image is outside STM32H755 flash: $data_load" >&2
    exit 1
}
in_range "$data_start" "$AXISRAM_BEGIN" "$AXISRAM_END" || {
    echo ".data start is outside AXI SRAM: $data_start" >&2
    exit 1
}
(( $((data_end)) >= $((data_start)) && $((data_end)) <= $((AXISRAM_END)) )) || {
    echo ".data end is invalid: $data_end" >&2
    exit 1
}
in_range "$bss_start" "$AXISRAM_BEGIN" "$AXISRAM_END" || {
    echo ".bss start is outside AXI SRAM: $bss_start" >&2
    exit 1
}
(( $((bss_end)) >= $((bss_start)) && $((bss_end)) <= $((AXISRAM_END)) )) || {
    echo ".bss end is invalid: $bss_end" >&2
    exit 1
}
[[ $((stack_top)) -eq $((AXISRAM_END)) ]] || {
    echo "Unexpected stack top: $stack_top (expected 0x24080000)" >&2
    exit 1
}
in_range "$stack_limit" "$AXISRAM_BEGIN" "$AXISRAM_END" || {
    echo "Stack limit is outside AXI SRAM: $stack_limit" >&2
    exit 1
}
[[ $((heap_limit)) -eq $((stack_limit)) ]] || {
    echo "Heap limit does not meet stack reservation: heap=$heap_limit stack=$stack_limit" >&2
    exit 1
}
(( $((heap_base)) <= $((heap_limit)) )) || {
    echo "Heap/static region overlaps reserved stack: heap=$heap_base limit=$heap_limit" >&2
    exit 1
}

printf 'vector=%s..%s\n' "$vector_start" "$vector_end"
printf '.data load=%s ram=%s..%s\n' "$data_load" "$data_start" "$data_end"
printf '.bss=%s..%s\n' "$bss_start" "$bss_end"
printf 'heap=%s..%s stack=%s..%s\n' "$heap_base" "$heap_limit" "$stack_limit" "$stack_top"
echo "RESULT: PASS"
