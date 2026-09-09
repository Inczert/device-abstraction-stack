# STM32H755 Cortex-M7 memory layout

DAS provides a reusable default linker script for bare-metal STM32H755 Cortex-M7 images:

```text
cmake/targets/stm32h755_cm7.ld
```

It replaces the linker script that would normally be generated or copied out of a CubeIDE/CubeMX project for the supported default layout. It is intentionally optional: a bootloader, RTOS, partitioned image, bank-specific update scheme, or application with special placement requirements may use its own linker script instead.

## Device memory map

The default script models the STM32H755 on-chip memories documented by ST in RM0399 and the STM32H755xI datasheet.

| Region | Origin | Size | Default use |
| --- | ---: | ---: | --- |
| ITCM | `0x00000000` | 64 KiB | exposed, no default section placement |
| FLASH | `0x08000000` | 2048 KiB | vector table, code, read-only data, initialized-data load image |
| DTCM | `0x20000000` | 128 KiB | exposed, no default section placement |
| AXISRAM | `0x24000000` | 512 KiB | `.data`, `.bss`, `.noinit`, heap bounds, stack |
| SRAM1 | `0x30000000` | 128 KiB | exposed, no default section placement |
| SRAM2 | `0x30020000` | 128 KiB | exposed, no default section placement |
| SRAM3 | `0x30040000` | 32 KiB | exposed, no default section placement |
| SRAM4 | `0x38000000` | 64 KiB | exposed, no default section placement |
| Backup SRAM | `0x38800000` | 4 KiB | exposed, no default section placement |

The two physical 1 MiB flash banks are represented as one contiguous 2 MiB `FLASH` region in the default script. Applications that need bank-specific placement should provide a custom linker script.

References:

- ST RM0399, *STM32H745/755 and STM32H747/757 advanced Arm-based 32-bit MCUs*, memory map table;
- ST STM32H755xI datasheet, embedded SRAM and flash-memory descriptions.

## Default section placement

The reusable script places:

```text
FLASH
  .isr_vector
  .text
  .rodata
  .ARM.extab
  .ARM.exidx
  load image of .data

AXISRAM
  .data
  .bss
  .noinit
  heap range
  reserved stack range at the top of AXI SRAM
```

The vector table starts at `0x08000000` and the output section is aligned to 1024 bytes so the script remains suitable for a complete STM32H755 vector table, not merely the small qualification table currently used by the test firmware.

Other memory regions are deliberately declared but are not populated automatically. DAS will not silently move data into TCM or shared SRAM and then require startup/cache/coherency machinery the application never asked for.

## Startup symbol contract

The linker script exports the symbols consumed by the reusable Cortex-M startup path:

```text
__data_load__
__data_start__
__data_end__
__bss_start__
__bss_end__
__vector_table_start__
```

Additional useful boundaries are exported:

```text
__vector_table_end__
__noinit_start__
__noinit_end__
__StackLimit
__StackTop
__HeapBase
__HeapLimit
__end__
```

The default stack top is:

```text
0x24080000
```

which is the end of AXI SRAM and matches the previously hardware-qualified DAS image.

## Stack and heap policy

The default linker script reserves 16 KiB at the top of AXI SRAM for the downward-growing Cortex-M stack:

```text
__stack_size__ = 16 KiB
```

The heap boundaries cover the remaining free AXI SRAM between static sections and the reserved stack:

```text
__HeapBase  = end of static/noinit data
__HeapLimit = __StackLimit
```

DAS does not provide a heap allocator merely because those boundaries exist.

The stack reservation can be changed at link time, for example:

```cmake
target_link_options(my_firmware PRIVATE
    -Wl,--defsym=__stack_size__=32768)
```

The linker asserts that static data/heap bounds do not overlap the reserved stack.

## Using the default linker target

DAS exposes an optional interface target:

```cmake
das::linker
```

A complete firmware using the default memory policy can link:

```cmake
target_link_libraries(my_firmware PRIVATE
    das::das
    das::linker)
```

`das::linker` propagates only the selected `-T` linker script. It does not force `--gc-sections`, libc selection, map-file naming, or other application link policy.

## Overriding the default script

There are two supported approaches.

### Select another script through DAS

Set `DAS_LINKER_SCRIPT` before adding DAS:

```cmake
set(DAS_LINKER_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/application.ld"
    CACHE FILEPATH "" FORCE)

add_subdirectory(third_party/device-abstraction-stack)

target_link_libraries(my_firmware PRIVATE das::das das::linker)
```

### Own the linker invocation completely

Do not link `das::linker`:

```cmake
target_link_libraries(my_firmware PRIVATE das::das)
target_link_options(my_firmware PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker/application.ld)
```

A custom script must provide whatever symbols are required by the startup implementation the application chooses. If the DAS Cortex-M `Reset_Handler` is used, it must satisfy the startup symbol contract above.

## Hardware qualification

The STM32H755 hardware campaign links its test image through `das::linker`. The old test-local linker script has been removed.

Before OpenOCD is started, the campaign checks the ELF and linker map for:

- required startup/memory symbols;
- vector table at `0x08000000`;
- initialized-data load image in flash;
- `.data` and `.bss` in AXI SRAM;
- stack top at `0x24080000`;
- valid heap/stack separation;
- a non-empty map containing the expected linker symbols.

The resulting `memory_layout.log`, linker map, ELF, symbol table, and the exact linker script are included in the campaign evidence archive.
