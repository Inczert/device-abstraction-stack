/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Default STM32H755 Cortex-M4 bare-metal memory layout.
 *
 * The dual-core-safe default keeps the CM4 image in flash bank 2 and ordinary
 * writable sections in D2 SRAM1. This deliberately avoids the CM7 default
 * image in flash bank 1 and AXI SRAM. Actual CM4 boot/release sequencing is a
 * separate dual-core concern and is not implied by this linker script.
 *
 * Applications with custom flash partitions, shared-memory windows, or another
 * RAM/stack policy should set DAS_LINKER_SCRIPT to their own linker script.
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH1   (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
    FLASH    (rx)  : ORIGIN = 0x08100000, LENGTH = 1024K
    SRAM1    (rwx) : ORIGIN = 0x30000000, LENGTH = 128K
    SRAM2    (rwx) : ORIGIN = 0x30020000, LENGTH = 128K
    SRAM3    (rwx) : ORIGIN = 0x30040000, LENGTH = 32K
    SRAM4    (rwx) : ORIGIN = 0x38000000, LENGTH = 64K
    BKPSRAM  (rwx) : ORIGIN = 0x38800000, LENGTH = 4K
}

PROVIDE(__stack_size__ = 16K);
__StackTop = ORIGIN(SRAM1) + LENGTH(SRAM1);
__StackLimit = __StackTop - __stack_size__;
__RAM_START__ = ORIGIN(SRAM1);
__RAM_END__ = ORIGIN(SRAM1) + LENGTH(SRAM1);
ASSERT(__StackLimit >= ORIGIN(SRAM1),
       "STM32H755 CM4 stack reservation exceeds SRAM1")

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
           "STM32H755 CM4 firmware requires a vector table")

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
    } > SRAM1 AT > FLASH
    __data_load__ = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(8);
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end__ = .;
    } > SRAM1

    .noinit (NOLOAD) :
    {
        . = ALIGN(8);
        __noinit_start__ = .;
        *(.noinit*)
        . = ALIGN(8);
        __noinit_end__ = .;
    } > SRAM1

    . = ALIGN(8);
    __end__ = .;
    __HeapBase = __end__;
    __HeapLimit = __StackLimit;

    ASSERT(__HeapBase <= __HeapLimit,
           "STM32H755 CM4 SRAM1 overflow: static data overlaps reserved stack")

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}
