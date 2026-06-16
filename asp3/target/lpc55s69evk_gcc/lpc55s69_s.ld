MEMORY
{
  /* Define each memory region */
  PROGRAM_FLASH (rx) : ORIGIN = 0x10000000, LENGTH = 0x4FE00 /* 319.5K bytes (alias Flash) */
  SG_veneer_table (rx) : ORIGIN = 0x1004FE00, LENGTH = 0x200 /* 512 bytes (alias Flash2) */
  Ram0 (rwx) : ORIGIN = 0x30000000, LENGTH = 0x20000 /* 128K bytes (alias RAM) */
  SRAMX (rwx) : ORIGIN = 0x14000000, LENGTH = 0x4000 /* 16K bytes (alias RAM2) */
}

/* Define a symbol for the top of each memory region */
__base_PROGRAM_FLASH = 0x10000000  ; /* PROGRAM_FLASH */
__base_Flash = 0x10000000 ; /* Flash */
__top_PROGRAM_FLASH = 0x10000000 + 0x4ea00 ; /* 314.5K bytes */
__top_Flash = 0x10000000 + 0x4ea00 ; /* 314.5K bytes */
__base_SG_veneer_table = 0x1004ea00  ; /* SG_veneer_table */
__base_Flash2 = 0x1004ea00 ; /* Flash2 */
__top_SG_veneer_table = 0x1004ea00 + 0x200 ; /* 512 bytes */
__top_Flash2 = 0x1004ea00 + 0x200 ; /* 512 bytes */
__base_Ram0 = 0x30000000  ; /* Ram0 */
__base_RAM = 0x30000000 ; /* RAM */
__top_Ram0 = 0x30000000 + 0x20000 ; /* 128K bytes */
__top_RAM = 0x30000000 + 0x20000 ; /* 128K bytes */
__base_SRAMX = 0x4000000  ; /* SRAMX */
__base_RAM2 = 0x14000000 ; /* RAM2 */
__top_SRAMX = 0x14000000 + 0x4000 ; /* 16K bytes */
__top_RAM2 = 0x14000000 + 0x4000 ; /* 16K bytes */

ENTRY(_kernel_start)

SECTIONS
{
    /* Place TrustZone Secure Gateway veneers in 'SG_veneer_table' memory */
    .gnu.sgstubs : ALIGN (32)
    {
        . = ALIGN(32);
        _start_sg = .;
        *(.gnu.sgstubs*)
        . = ALIGN(32);
        _end_sg = .;
    } > SG_veneer_table
    .text_Flash2 : ALIGN(4)
    {
       FILL(0xff)
        *(.text_Flash2) /* for compatibility with previous releases */
        *(.text_SG_veneer_table) /* for compatibility with previous releases */
        *(.text.$Flash2)
        *(.text.$SG_veneer_table)
        *(.text_Flash2.*) /* for compatibility with previous releases */
        *(.text_SG_veneer_table.*) /* for compatibility with previous releases */
        *(.text.$Flash2.*)
        *(.text.$SG_veneer_table.*)
        *(.rodata.$Flash2)
        *(.rodata.$SG_veneer_table)
        *(.rodata.$Flash2.*)
        *(.rodata.$SG_veneer_table.*)
    } > SG_veneer_table

    /* MAIN TEXT SECTION */
    .text : ALIGN(4)
    {
        FILL(0xff)
        __vectors_start__ = ABSOLUTE(.) ;
        KEEP(*(.vector))
        /* Global Section Table */
        . = ALIGN(4) ;
        __section_table_start = .;
        __data_section_table = .;
        LONG(LOADADDR(.data));
        LONG(    ADDR(.data));
        LONG(  SIZEOF(.data));
        __data_section_table_end = .;
        __bss_section_table = .;
        LONG(    ADDR(.bss));
        LONG(  SIZEOF(.bss));
        __bss_section_table_end = .;
        __section_table_end = . ;
        /* End of Global Section Table */
    } > PROGRAM_FLASH

    .text : ALIGN(4)
    {
       *(.text*)
       *(.rodata .rodata.* .constdata .constdata.*)
       KEEP(*(.empty.*))
       . = ALIGN(4);
    } > PROGRAM_FLASH
    /*
     * for exception handling/unwind - some Newlib functions (in common
     * with C++ and STDC++) use this. 
     */
    .ARM.extab : ALIGN(4) 
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > PROGRAM_FLASH

    __exidx_start = .;

    .ARM.exidx : ALIGN(4)
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > PROGRAM_FLASH
    __exidx_end = .;
 
    _etext = .;

    /* DATA section for SRAMX */
    .data_RAM2 : ALIGN(4)
    {
        FILL(0xff)
        PROVIDE(__start_data_RAM2 = .) ;
        *(.ramfunc.$RAM2)
        *(.ramfunc.$SRAMX)
        *(.data.$RAM2)
        *(.data.$SRAMX)
        *(.data.$RAM2.*)
        *(.data.$SRAMX.*)
        . = ALIGN(4) ;
        PROVIDE(__end_data_RAM2 = .) ;
    } > SRAMX AT>PROGRAM_FLASH

    /* MAIN DATA SECTION */
    .uninit_RESERVED (NOLOAD) : ALIGN(4)
    {
        _start_uninit_RESERVED = .;
        KEEP(*(.bss.$RESERVED*))
       . = ALIGN(4) ;
        _end_uninit_RESERVED = .;
    } > Ram0 AT> Ram0

    /* Main DATA section (Ram0) */
    .data : ALIGN(4)
    {
       FILL(0xff)
       _data = . ;
       PROVIDE(__start_data_RAM = .) ;
       PROVIDE(__start_data_Ram0 = .) ;
       *(vtable)
       *(.ramfunc*)
       KEEP(*(CodeQuickAccess))
       KEEP(*(DataQuickAccess))
       *(RamFunction)
       *(.data*)
       . = ALIGN(4) ;
       _edata = . ;
       PROVIDE(__end_data_RAM = .) ;
       PROVIDE(__end_data_Ram0 = .) ;
    } > Ram0 AT>PROGRAM_FLASH

    /* BSS section for SRAMX */
    .bss_RAM2 :
    {
       . = ALIGN(4) ;
       PROVIDE(__start_bss_RAM2 = .) ;
       *(.bss.$RAM2)
       *(.bss.$SRAMX)
       *(.bss.$RAM2.*)
       *(.bss.$SRAMX.*)
       . = ALIGN (. != 0 ? 4 : 1) ; /* avoid empty segment */
       PROVIDE(__end_bss_RAM2 = .) ;
    } > SRAMX

    /* MAIN BSS SECTION */
    .bss : ALIGN(4)
    {
        _bss = .;
        PROVIDE(__start_bss_RAM = .) ;
        PROVIDE(__start_bss_Ram0 = .) ;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4) ;
        _ebss = .;
        PROVIDE(__end_bss_RAM = .) ;
        PROVIDE(__end_bss_Ram0 = .) ;
        PROVIDE(end = .);
    } > Ram0 AT> Ram0

    /* NOINIT section for SRAMX */
    .noinit_RAM2 (NOLOAD) :
    {
       . = ALIGN(4) ;
       *(.noinit.$RAM2)
       *(.noinit.$SRAMX)
       *(.noinit.$RAM2.*)
       *(.noinit.$SRAMX.*)
       . = ALIGN(4) ;
    } > SRAMX

    /* DEFAULT NOINIT SECTION */
    .noinit (NOLOAD): ALIGN(4)
    {
        _noinit = .;
        PROVIDE(__start_noinit_RAM = .) ;
        PROVIDE(__start_noinit_Ram0 = .) ;
        *(.noinit*)
         . = ALIGN(4) ;
        _end_noinit = .;
       PROVIDE(__end_noinit_RAM = .) ;
       PROVIDE(__end_noinit_Ram0 = .) ;        
    } > Ram0 AT> Ram0

    /* Reserve and place Heap within memory map */
    _HeapSize = 0x800;
    .heap :  ALIGN(4)
    {
        _pvHeapStart = .;
        . += _HeapSize;
        . = ALIGN(4);
        _pvHeapLimit = .;
    } > Ram0

     _StackSize = 0x800;
     /* Reserve space in memory for Stack */
    .heap2stackfill  :
    {
        . += _StackSize;
    } > Ram0
    /* Locate actual Stack in memory map */
    .stack ORIGIN(Ram0) + LENGTH(Ram0) - _StackSize - 0:  ALIGN(4)
    {
        _vStackBase = .;
        . = ALIGN(4);
        _vStackTop = . + _StackSize;
    } > Ram0

    /* Provide basic symbols giving location and size of main text
     * block, including initial values of RW data sections. Note that
     * these will need extending to give a complete picture with
     * complex images (e.g multiple Flash banks).
     */
    _image_start = LOADADDR(.text);
    _image_end = LOADADDR(.data) + SIZEOF(.data);
    _image_size = _image_end - _image_start;
}
