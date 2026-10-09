OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(Reset_Handler)

_Min_Heap_Size  = 0x00000000;
_Min_Stack_Size = 0x00001000; /* 4 KB reserved stack budget */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 16K
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 20K
}

_estack      = ORIGIN(RAM) + LENGTH(RAM);
__StackTop   = _estack;
__StackLimit = __StackTop - _Min_Stack_Size;

SECTIONS
{
    .isr_vector ORIGIN(FLASH) :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text.Reset_Handler)
        *(.text.Default_Handler)
        *(.text .text.*)
        *(.glue_7 .glue_7t)
        *(.vfp11_veneer)
        *(.v4_bx)

        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > FLASH

    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data .data.*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    _sidata = LOADADDR(.data);
    __flash_image_end = LOADADDR(.data) + SIZEOF(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = .;
    } > RAM

    .noinit (NOLOAD) :
    {
        . = ALIGN(4);
        *(.noinit .noinit.*)
        . = ALIGN(4);
        _end = .;
        PROVIDE(end = .);
    } > RAM

    __HeapBase  = _end;
    __HeapLimit = __StackLimit;

    .ARM.attributes 0 :
    {
        *(.ARM.attributes)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}

ASSERT(ADDR(.isr_vector) == ORIGIN(FLASH),
       "Bootloader vector table must start at 0x08000000")
ASSERT(SIZEOF(.isr_vector) == 0xEC,
       "Unexpected vector table size for STM32F103 medium-density")
ASSERT(__flash_image_end <= (ORIGIN(FLASH) + LENGTH(FLASH)),
       "Bootloader exceeds its 16 KiB Flash region")
ASSERT(_end <= __StackLimit,
       "Bootloader RAM overlaps the reserved 4 KB stack")
ASSERT((__StackLimit - _end) >= _Min_Heap_Size,
       "Bootloader RAM does not satisfy the heap budget")
