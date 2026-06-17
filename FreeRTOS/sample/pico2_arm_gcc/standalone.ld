/*
 *  Non-secure リンカスクリプト（Raspberry Pi Pico2 / RP2350, SafeG-M FreeRTOS）
 *
 *  ベアメタル版 test/ns_baremetal/pico2_arm_gcc/ns_pico2.ld と同じ NS 領域に配置。
 *  Secure 側 SAU/ld と整合:
 *    NS code : 0x10200000.. (TOPPERS_NS_VTOR, SAU R1)
 *    NS RAM  : 0x20040000.. (SAU R2, Secure RAM 0x20000000-0x2003FFFF の後)
 *  ベクタ先頭 [0]=初期MSP, [1]=ResetISR を launch_ns が参照する。
 *  FreeRTOS heap(heap_4)/タスクスタックは configTOTAL_HEAP_SIZE の静的配列(.bss)から
 *  確保するため、専用 heap 領域は不要。
 */

MEMORY
{
    NS_FLASH (rx)  : ORIGIN = 0x10200000, LENGTH = 1024K
    NS_RAM   (rwx) : ORIGIN = 0x20040000, LENGTH = 256K
}

ENTRY(ResetISR)

SECTIONS
{
    .text : ALIGN(4)
    {
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata .rodata.* .constdata .constdata.*)
        . = ALIGN(4);
    } > NS_FLASH

    .ARM.exidx : ALIGN(4)
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > NS_FLASH

    __data_load = LOADADDR(.data);
    .data : ALIGN(4)
    {
        __data_start = .;
        *(vtable)
        *(.data*)
        . = ALIGN(4);
        __data_end = .;
    } > NS_RAM AT> NS_FLASH

    .bss (NOLOAD) : ALIGN(4)
    {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > NS_RAM

    /* 初期スタックポインタ = NS_RAM の末尾 */
    __stack_top = ORIGIN(NS_RAM) + LENGTH(NS_RAM);
}
