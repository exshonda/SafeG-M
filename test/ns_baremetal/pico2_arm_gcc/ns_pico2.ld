/*
 *  Non-secure 側リンカスクリプト（Raspberry Pi Pico2 / RP2350, SafeG-M テスト）
 *
 *  RP2350 は an505 のような NS エイリアス空間を持たず、SAU が同一アドレスを
 *  NS 指定する。Secure 側 SAU/ld と整合:
 *    NS code : 0x10200000.. (TOPPERS_NS_VTOR, SAU R1 が NS 指定)
 *    NS RAM  : 0x20040000.. (SAU R2 が NS 指定, Secure RAM 0x20000000-0x2003FFFF の後)
 *  ベクタテーブル先頭 [0]=初期MSP, [1]=ResetISR を launch_ns が参照する。
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
        KEEP(*(.ns_vector))
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
