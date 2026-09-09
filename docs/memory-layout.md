# STM32H755 memory and linker policy

DAS provides default linker scripts for both CPU cores of the STM32H755. They are build-time device/core policy, not part of the generic Cortex-M implementation.

```text
cmake/targets/stm32h755_cm7.ld
cmake/targets/stm32h755_cm4.ld
```

The defaults are intended to make a simple bare-metal image link without CubeIDE/CubeMX-generated files while remaining easy to replace for a real product.

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

`DAS_CORE` also selects the compiler/FPU flags, CMSIS core header, `CORE_CM7`/`CORE_CM4` device definition, and the default linker script.

## Default dual-core partition

The STM32H755 contains 2 MiB of internal flash arranged as two 1 MiB banks. DAS uses a deliberately non-overlapping default:

| Core | Code/vector flash | Writable default RAM | Stack top |
| --- | --- | --- | --- |
| CM7 | bank 1, `0x08000000..0x080FFFFF` | AXI SRAM, `0x24000000..0x2407FFFF` | `0x24080000` |
| CM4 | bank 2, `0x08100000..0x081FFFFF` | D2 SRAM1, `0x30000000..0x3001FFFF` | `0x30020000` |

This is a safe default partition, not a claim that each core can only access those regions. The split simply prevents two independently linked default images from owning the same flash and RAM ranges.

### CM7 script

`stm32h755_cm7.ld` places the vector table, code, read-only data and `.data` load image in flash bank 1. Normal writable sections, heap bounds and stack are placed in AXI SRAM.

### CM4 script

`stm32h755_cm4.ld` places the vector table, code, read-only data and `.data` load image in flash bank 2. Normal writable sections, heap bounds and stack are placed in D2 SRAM1.

The CM4 default boot address therefore matches the second flash bank at `0x08100000`.

## Startup contract

Both scripts export the symbols consumed by the reusable Cortex-M reset path:

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
```

The Cortex-M startup code knows what these symbols mean. It does not know their STM32 addresses.

```text
Cortex-M startup mechanics    src/mcu/cortex_m/
STM32H755 memory addresses    cmake/targets/stm32h755_*.ld
```

## How the linker script reaches the firmware

`libdas.a` itself is a static archive. It is not assigned final flash/RAM addresses when the archive is created.

The linker script matters when the final executable is linked:

```text
DAS objects ─┐
app objects ─┼─> final link + selected .ld ─> firmware.elf
libdas.a ────┘
```

DAS attaches the selected linker script as an **INTERFACE link option** of `das::das`. A normal application needs only:

```cmake
target_link_libraries(my_firmware PRIVATE das::das)
```

There is no `das::linker` library target and no firmware-configuration helper.

## Custom linker script

The default can be replaced at configure time:

```bash
cmake ... \
  -DDAS_DEVICE=nucleo_h755zi_q \
  -DDAS_CORE=cm7 \
  -DDAS_LINKER_SCRIPT=/absolute/or/relative/path/application.ld
```

or from a parent `CMakeLists.txt`:

```cmake
set(DAS_LINKER_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/application.ld"
    CACHE FILEPATH "Application linker script" FORCE)

add_subdirectory(third_party/device-abstraction-stack)
target_link_libraries(my_firmware PRIVATE das::das)
```

An empty `DAS_LINKER_SCRIPT` means: use the default selected by `DAS_DEVICE + DAS_CORE`.

Custom layouts are expected for bootloaders, A/B image slots, TCM placement, external RAM, DMA/non-cacheable sections, shared-memory windows, or different stack/heap policy.

If the DAS reusable startup is retained, the custom script must still provide its linker-symbol contract. If the application also replaces startup, it may define a completely different contract.

## Custom-linker qualification fixture

The repository contains a test-only linker script:

```text
tests/link/stm32h755/custom_cm7.ld
```

It deliberately relocates the CM7 vector table and image start from `0x08000000` to:

```text
0x08020000
```

The campaign configures a separate build with:

```text
DAS_CORE=cm7
DAS_LINKER_SCRIPT=tests/link/stm32h755/custom_cm7.ld
```

and links the normal `das::das` consumer test without any explicit `-T` option in that target. The generated ELF/map are then checked against the relocated flash range.

This proves two things independently:

1. `DAS_LINKER_SCRIPT` really overrides the device/core default;
2. the override reaches the final executable transitively through `das::das`.

The fixture is not flashed to hardware. Its purpose is to validate build-system/linker selection, not to consume another flash sector for ceremonial reasons.

## Stack and heap

Both default scripts reserve 16 KiB for the reset/application stack. The value can be overridden at final link time:

```cmake
target_link_options(my_firmware PRIVATE
    -Wl,--defsym=__stack_size__=32768)
```

The scripts assert that static data/heap bounds do not overlap the reserved stack.

DAS does not provide a heap allocator. `__HeapBase` and `__HeapLimit` are boundaries available to an application/runtime that chooses to use them.

## Qualification

The full campaign validates three link configurations:

```text
CM7 default layout      static + physical execution
CM4 default layout      static + physical execution
CM7 custom override     static linker-selection test
```

Physical execution of the CM4 image proves that the selected bank-2/D2-SRAM layout can actually boot under debugger control and reconstruct the C runtime correctly.

That still does not qualify production CM7-to-CM4 release sequencing, HSEM or shared-memory ownership. Those are dual-core system behaviors rather than linker behaviors.
