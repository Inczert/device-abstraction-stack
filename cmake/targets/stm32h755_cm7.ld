/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Default STM32H755 Cortex-M7 bare-metal memory layout.
 *
 * This script intentionally models the complete on-chip RAM map but only places
 * ordinary sections in FLASH and AXI SRAM. Applications needing a bootloader,
 * bank split, TCM placement, shared-memory windows, or a different stack policy
 * should supply their own linker script instead of using das::linker.
 */

ENTRY(Reset_Handler)

MEMORY
{
    ITCM    (rwx) : ORIGIN = 0x00000000, LENGTH = 64K
    FLASH   (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
    DTCM    (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
    AXISRAM (rwx) : ORIGIN = 0x24000000, LENGTH = 512K
    SRAM1   (rwx) : ORIGIN = 0x30000000, LENGTH = 128K
    SRAM2   (rwx) : ORIGIN = 0x30020000, LENGTH = 128K
    SRAM3   (rwx) : ORIGIN = 0x30040000, LENGTH = 32K
    SRAM4   (rwx) : ORIGIN = 0x38000000, LENGTH = 64K
    BKPSRAM (rwx) : ORIGIN = 0x38800000, LENGTH = 4K
}

/*
 * Keep the reset stack at the top of AXI SRAM, matching the previously
 * qualified DAS STM32H755 image. The default reserved size can be overridden
 * by defining __stack_size__ before this script is evaluated, for example with
 * -Wl,--defsym=__stack_size__=32768.
 */
PROVIDE(__stack_size__ = 16K);
__StackTop = ORIGIN(AXISRAM) + LENGTH(AXISRAM);
__StackLimit = __StackTop - __stack_size__;
ASSERT(__StackLimit >= ORIGIN(AXISRAM), "STM32H755 stack reservation exceeds AXI SRAM")

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
           "STM32H755 AXI SRAM overflow: static data overlaps reserved stack")

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}
