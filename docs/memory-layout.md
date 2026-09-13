# STM32H755 memory and linker policy

DAS provides default linker scripts for both CPU cores of the STM32H755. They are build-time device/core policy, not part of the generic Cortex-M implementation.

```text
cmake/targets/stm32h755_cm7.ld
cmake/targets/stm32h755_cm4.ld
```

The defaults make a simple bare-metal image link without CubeIDE/CubeMX-generated files while remaining replaceable for a product-specific layout.

## Core selection

The NUCLEO-H755ZI-Q target is selected with both a board and a CPU core:

```cmake
-DAS_DEVICE=nucleo_h755zi_q
-DDAS_CORE=cm7
```

or:

```cmake
-DAS_DEVICE=nucleo_h755zi_q
-DDAS_CORE=cm4
```

`DAS_CORE` also selects compiler/FPU flags, CMSIS core definitions, the `CORE_CM7`/`CORE_CM4` device view and the default linker script.

## Default dual-core partition

The STM32H755 contains 2 MiB of internal flash arranged as two 1 MiB banks. DAS uses a deliberately non-overlapping default:

| Core | Code/vector flash | Writable default RAM | Stack top |
| --- | --- | --- | --- |
| CM7 | bank 1, `0x08000000..0x080FFFFF` | AXI SRAM, `0x24000000..0x2407FFFF` | `0x24080000` |
| CM4 | bank 2, `0x08100000..0x081FFFFF` | D2 SRAM1, `0x30000000..0x3001FFFF` | `0x30020000` |

This is a safe default partition, not a claim that each core can only access those regions. The split prevents two independently linked default images from owning the same flash/RAM ranges.

The CM7 AXI-SRAM default is also accessible by the STM32H755 Ethernet DMA engine and is therefore suitable for the current Ethernet descriptor/frame-buffer placement. DTCM is not accessible to Ethernet DMA and must not be used for those objects.

## Linker/runtime contract

Both default scripts export the symbols consumed by DAS startup and by integrations that need the selected writable-RAM bounds:

```text
__vector_table_start__
__vector_table_end__

__data_load__
__data_start__
__data_end__

__bss_start__
__bss_end__

__noinit_start__
__noinit_end__

__HeapBase
__HeapLimit

__StackLimit
__StackTop

__RAM_START__
__RAM_END__
```

`__RAM_START__` / `__RAM_END__` describe the default writable region selected for the configured core. HardRT uses these bounds for task-stack validation. A custom linker used with that integration must provide equivalent symbols.

The Cortex-M startup code consumes the runtime symbols without knowing STM32 physical addresses:

```text
Cortex-M startup mechanics    src/mcu/cortex_m/
STM32H755 memory addresses    cmake/targets/stm32h755_*.ld
```

## How the linker script reaches the firmware

`libdas.a` is a static archive and has no final physical addresses by itself. The selected linker script applies when the firmware executable is linked:

```text
DAS objects ─┐
app objects ─┼─> final link + selected .ld ─> firmware.elf
libdas.a ────┘
```

DAS attaches the selected linker script as an **INTERFACE link option** of `das::das`. A normal application needs only:

```cmake
target_link_libraries(my_firmware PRIVATE das::das)
```

There is no linker pseudo-library and no required firmware-configuration helper.

## Custom linker script

The default can be replaced at configure time:

```bash
cmake ... \
  -DDAS_DEVICE=nucleo_h755zi_q \
  -DDAS_CORE=cm7 \
  -DDAS_LINKER_SCRIPT=/absolute/or/relative/path/application.ld
```

or from a parent project:

```cmake
set(DAS_LINKER_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/application.ld"
    CACHE FILEPATH "Application linker script" FORCE)

add_subdirectory(third_party/device-abstraction-stack)
target_link_libraries(my_firmware PRIVATE das::das)
```

An empty `DAS_LINKER_SCRIPT` means use the default selected by `DAS_DEVICE + DAS_CORE`.

Custom layouts are expected for bootloaders, A/B slots, TCM placement, external RAM, DMA/non-cacheable sections, shared-memory windows or different stack/heap policy.

If DAS startup is retained, the custom script must provide its startup-symbol contract. Integrations may impose additional symbols such as `__RAM_START__` / `__RAM_END__`.

If Ethernet is used, the custom layout must place Ethernet descriptors/buffers in SRAM reachable by the Ethernet DMA engine. Merely being CPU-accessible is not sufficient.

## Custom-linker qualification fixture

The repository contains:

```text
tests/link/stm32h755/custom_cm7.ld
```

It deliberately relocates the CM7 image/vector start to `0x08020000`. The campaign links the normal `das::das` consumer against this script and checks the generated ELF/map, proving both the override and its transitive propagation through the target.

The fixture is static-only; it is not flashed.

## Stack and heap

Both defaults reserve 16 KiB for the reset/application stack. The size may be overridden at final link time:

```cmake
target_link_options(my_firmware PRIVATE
    -Wl,--defsym=__stack_size__=32768)
```

The scripts assert that static data/heap bounds do not overlap the reserved stack.

DAS does not provide a heap allocator. `__HeapBase` and `__HeapLimit` are boundaries for an application/runtime that chooses to use them.

## Qualification

The standing campaign validates:

```text
CM7 default layout      static + physical execution
CM4 default layout      static + physical execution
CM7 custom override     static linker-selection test
```

The current full hardware baseline is **39/39 PASS** at DAS commit `f6b65672d9ae69cf28cd574d0dbba01cf875d8dc` (2026-09-13). Physical CM4 execution proves the bank-2/D2-SRAM default image boots under debugger control and reconstructs the C runtime. CM7 Ethernet qualification additionally exercises Ethernet-DMA-visible storage under the default AXI-SRAM policy.

This still does not qualify production CM7-to-CM4 release sequencing, HSEM or shared-memory ownership; those are system-lifecycle concerns rather than linker behavior.
