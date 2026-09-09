#!/usr/bin/env bash
set -Eeuo pipefail

CORE=""

usage() {
    echo "Usage: $0 --core <cm7|cm4> <firmware.elf> <firmware.map>" >&2
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --core) CORE="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        --*) echo "Unknown option: $1" >&2; usage; exit 2 ;;
        *) break ;;
    esac
done

[[ "$CORE" == "cm7" || "$CORE" == "cm4" ]] || {
    echo "--core must be cm7 or cm4" >&2
    usage
    exit 2
}
[[ $# -eq 2 ]] || { usage; exit 2; }

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

case "$CORE" in
    cm7)
        FLASH_BEGIN=0x08000000
        FLASH_END=0x08100000
        RAM_BEGIN=0x24000000
        RAM_END=0x24080000
        RAM_NAME="AXI SRAM"
        ;;
    cm4)
        FLASH_BEGIN=0x08100000
        FLASH_END=0x08200000
        RAM_BEGIN=0x30000000
        RAM_END=0x30020000
        RAM_NAME="D2 SRAM1"
        ;;
esac

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

[[ $((vector_start)) -eq $((FLASH_BEGIN)) ]] || {
    printf 'Unexpected %s vector-table address: %s (expected 0x%08x)\n' \
        "$CORE" "$vector_start" "$((FLASH_BEGIN))" >&2
    exit 1
}

(( $((vector_end)) >= $((vector_start)) && $((vector_end)) <= $((FLASH_END)) )) || {
    echo "Vector table ends outside $CORE flash allocation: $vector_end" >&2
    exit 1
}
in_range "$data_load" "$FLASH_BEGIN" "$FLASH_END" || {
    echo ".data load image is outside $CORE flash allocation: $data_load" >&2
    exit 1
}
in_range "$data_start" "$RAM_BEGIN" "$RAM_END" || {
    echo ".data start is outside $RAM_NAME: $data_start" >&2
    exit 1
}
(( $((data_end)) >= $((data_start)) && $((data_end)) <= $((RAM_END)) )) || {
    echo ".data end is invalid for $RAM_NAME: $data_end" >&2
    exit 1
}
in_range "$bss_start" "$RAM_BEGIN" "$RAM_END" || {
    echo ".bss start is outside $RAM_NAME: $bss_start" >&2
    exit 1
}
(( $((bss_end)) >= $((bss_start)) && $((bss_end)) <= $((RAM_END)) )) || {
    echo ".bss end is invalid for $RAM_NAME: $bss_end" >&2
    exit 1
}
[[ $((stack_top)) -eq $((RAM_END)) ]] || {
    printf 'Unexpected %s stack top: %s (expected 0x%08x)\n' \
        "$CORE" "$stack_top" "$((RAM_END))" >&2
    exit 1
}
in_range "$stack_limit" "$RAM_BEGIN" "$RAM_END" || {
    echo "Stack limit is outside $RAM_NAME: $stack_limit" >&2
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

printf 'core=%s\n' "$CORE"
printf 'vector=%s..%s\n' "$vector_start" "$vector_end"
printf '.data load=%s ram=%s..%s\n' "$data_load" "$data_start" "$data_end"
printf '.bss=%s..%s\n' "$bss_start" "$bss_end"
printf 'heap=%s..%s stack=%s..%s\n' "$heap_base" "$heap_limit" "$stack_limit" "$stack_top"
echo "RESULT: PASS"
