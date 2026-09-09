ENTRY(Reset_Handler)

MEMORY
{
    FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
    AXIRAM (rwx) : ORIGIN = 0x24000000, LENGTH = 512K
}

__StackTop = ORIGIN(AXIRAM) + LENGTH(AXIRAM);

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(256);
        KEEP(*(.isr_vector))
    } > FLASH

    .text :
    {
        *(.text*)
        *(.rodata*)
        KEEP(*(.init))
        KEEP(*(.fini))
    } > FLASH

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
    __exidx_start = .;
    .ARM.exidx : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } > FLASH
    __exidx_end = .;

    .data :
    {
        . = ALIGN(8);
        __data_start__ = .;
        *(.data*)
        __data_end__ = .;
    } > AXIRAM AT > FLASH
    __data_load__ = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(8);
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        __bss_end__ = .;
    } > AXIRAM

    . = ALIGN(8);
    __end__ = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}
