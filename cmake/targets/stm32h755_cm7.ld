/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Default STM32H755 Cortex-M7 bare-metal memory layout.
 *
 * The dual-core-safe default keeps the CM7 image in flash bank 1 and ordinary
 * writable sections in AXI SRAM. Flash bank 2 and D2 SRAM remain available for
 * a CM4 image/shared-memory policy. Applications with a bootloader, custom
 * partitions, TCM placement, external RAM, or another stack policy should set
 * DAS_LINKER_SCRIPT to their own linker script.
 */

ENTRY(Reset_Handler)

MEMORY
{
    ITCM     (rwx) : ORIGIN = 0x00000000, LENGTH = 64K
    FLASH    (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
    FLASH2   (rx)  : ORIGIN = 0x08100000, LENGTH = 1024K
    DTCM     (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
    AXISRAM  (rwx) : ORIGIN = 0x24000000, LENGTH = 512K
    SRAM1    (rwx) : ORIGIN = 0x30000000, LENGTH = 128K
    SRAM2    (rwx) : ORIGIN = 0x30020000, LENGTH = 128K
    SRAM3    (rwx) : ORIGIN = 0x30040000, LENGTH = 32K
    SRAM4    (rwx) : ORIGIN = 0x38000000, LENGTH = 64K
    BKPSRAM  (rwx) : ORIGIN = 0x38800000, LENGTH = 4K
}

PROVIDE(__stack_size__ = 16K);
__StackTop = ORIGIN(AXISRAM) + LENGTH(AXISRAM);
__StackLimit = __StackTop - __stack_size__;
__RAM_START__ = ORIGIN(AXISRAM);
__RAM_END__ = ORIGIN(AXISRAM) + LENGTH(AXISRAM);
ASSERT(__StackLimit >= ORIGIN(AXISRAM),
       "STM32H755 CM7 stack reservation exceeds AXI SRAM")

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(1024);
        __vector_table_start__ = .;
        KEEP(*(.isr_vector))
        __vector_table_end__ = .;
    } > FLASH
    ASSERT(SIZEOF(.isr_vector) >= 8,
           "STM32H755 CM7 firmware requires a vector table")

    .text :
    {
        . = ALIGN(4);
        *(.text*)
        *(.rodata*)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        __etext = .;
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > FLASH
    __exidx_end = .;

    .data :
    {
        . = ALIGN(8);
        __data_start__ = .;
        *(.data*)
        . = ALIGN(8);
        __data_end__ = .;
    } > AXISRAM AT > FLASH
    __data_load__ = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(8);
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end__ = .;
    } > AXISRAM

    .noinit (NOLOAD) :
    {
        . = ALIGN(8);
        __noinit_start__ = .;
        *(.noinit*)
        . = ALIGN(8);
        __noinit_end__ = .;
    } > AXISRAM

    . = ALIGN(8);
    __end__ = .;
    __HeapBase = __end__;
    __HeapLimit = __StackLimit;

    ASSERT(__HeapBase <= __HeapLimit,
           "STM32H755 CM7 AXI SRAM overflow: static data overlaps reserved stack")

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}
