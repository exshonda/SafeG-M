/*
 *  リンカスクリプト（ARM MPS2-AN505 / QEMU 用, Secure・SafeG-M 構成）
 *
 *  SSE-200 / IoTKit メモリマップ（QEMU hw/arm/mps2-tz.c, armsse.c）:
 *    0x10000000.. : コード用 SSRAM（Secure，0x00000000 の NS エイリアスと同一実体, 4MB）
 *    0x38000000.. : データ用 SSRAM（Secure，0x28000000 の NS エイリアスと同一実体, 4MB）
 *
 *  SafeG-M の Secure/NS 分割（mimxrt685_s.ld を踏襲し AN505 アドレスへ移植）:
 *    Secure code : 0x10000000 .. 0x101FFDFF (約 2MB)
 *    NSC veneer  : 0x101FFE00 .. 0x101FFFFF (512B, .gnu.sgstubs)
 *    NS  code    : 0x10200000.. (= NS エイリアス 0x00200000, TOPPERS_NS_VTOR)
 *    Secure RAM  : 0x38000000 .. 0x381FFFFF (2MB)
 *    NS  RAM     : 0x38200000.. (= NS エイリアス 0x28200000)
 *
 *  共通スタートアップ start.S が参照するシンボル
 *  （__idata_start/__idata_end/__data_start/__bss_start/__bss_end）を定義する．
 */

MEMORY
{
    FLASH    (rx)  : ORIGIN = 0x10000000, LENGTH = 0x1FFE00  /* Secure code (~2MB) */
    SG_veneer(rx)  : ORIGIN = 0x101FFE00, LENGTH = 0x200     /* NSC veneer 512B    */
    RAM      (rwx) : ORIGIN = 0x38000000, LENGTH = 0x200000  /* Secure RAM 2MB     */
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

    /* TrustZone Secure Gateway veneers を NSC 領域に配置 */
    .gnu.sgstubs : ALIGN(32)
    {
        . = ALIGN(32);
        *(.gnu.sgstubs*)
        . = ALIGN(32);
    } > SG_veneer

    __idata_start = LOADADDR(.data);
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
