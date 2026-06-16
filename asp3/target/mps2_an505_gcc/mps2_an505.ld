/*
 *  リンカスクリプト（ARM MPS2-AN505 / QEMU 用, Secure・非SafeG構成）
 *
 *  SSE-200 / IoTKit メモリマップ（QEMU hw/arm/mps2-tz.c, armsse.c）:
 *    0x10000000.. : コード用 SSRAM（0x00000000 のセキュアエイリアス）
 *    0x38000000.. : データ用 SSRAM（0x28000000 のセキュアエイリアス）
 *  共通スタートアップ start.S が参照するシンボル
 *  （__idata_start/__idata_end/__data_start/__bss_start/__bss_end）を定義する．
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x10000000, LENGTH = 4096K
    RAM   (rwx) : ORIGIN = 0x38000000, LENGTH = 4096K
}

ENTRY(_kernel_start)

SECTIONS
{
    .text : ALIGN(4)
    {
        KEEP(*(.vector))
        *(.text*)
        *(.rodata .rodata.* .constdata .constdata.*)
        KEEP(*(.empty.*))
        . = ALIGN(4);
    } > FLASH

    .ARM.extab : ALIGN(4) { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
    .ARM.exidx : ALIGN(4)
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > FLASH

    __idata_start = .;
    .data : ALIGN(4)
    {
        __data_start = .;
        *(vtable)
        *(.data*)
        . = ALIGN(4);
    } > RAM AT> FLASH
    __idata_end = __idata_start + SIZEOF(.data);

    .bss (NOLOAD) : ALIGN(4)
    {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
        PROVIDE(end = .);
    } > RAM
}
