/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Test-only STM32H755 CM7 custom linker layout.
 *
 * This deliberately relocates the vector table and image start to the second
 * 128 KiB sector of flash bank 1. It exists to prove that DAS_LINKER_SCRIPT
 * overrides the core-selected default when an application links das::das.
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH   (rx)  : ORIGIN = 0x08020000, LENGTH = 896K
    AXISRAM (rwx) : ORIGIN = 0x24000000, LENGTH = 512K
}

PROVIDE(__stack_size__ = 16K);
__StackTop = ORIGIN(AXISRAM) + LENGTH(AXISRAM);
__StackLimit = __StackTop - __stack_size__;
ASSERT(__StackLimit >= ORIGIN(AXISRAM), "custom CM7 stack reservation exceeds AXI SRAM")

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(1024);
        __vector_table_start__ = .;
        KEEP(*(.isr_vector))
        __vector_table_end__ = .;
    } > FLASH

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
           "custom CM7 AXI SRAM overflow: static data overlaps reserved stack")

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}
