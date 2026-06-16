/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
 *
 *  Copyright (C) 2026 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，本ソフトウェアを TOPPERS ライセンス（条件は他のソー
 *  スファイルの先頭コメントを参照）の下で利用することを許諾する．本ソフ
 *  トウェアは無保証で提供される．
 *
 */

/*
 * ARM CMSDK APB UART 用 簡易SIOドライバ（TECS版, MPS2-AN505 / QEMU）
 *
 *  ASP3CORE の非TECS版 cmsdk_uart.c のレジスタ処理を，SafeG-M の TECS
 *  SIO コンポーネント（tUsart）に移植したもの．レジスタ定義は
 *  mps2_an505.h を参照．送受信は CMSDK の DATA/STATE/CTRL/INTSTATUS で
 *  ポーリング＋割込みを行う．
 */

#include <sil.h>
#include "mps2_an505.h"
#include "tUsart_tecsgen.h"
#include "kernel/kernel_impl.h"

/*
 * シリアルI/Oポートのオープン
 */
void eSIOPort_open(CELLIDX idx)
{
    CELLCB *p_cellcb = GET_CELLCB(idx);
    uint32_t base = ATTR_base;

    /* いったん停止し，割込みステータスをすべてクリア */
    sil_wrw_mem((void *) CMSDK_UART_CTRL(base), 0U);
    sil_wrw_mem((void *) CMSDK_UART_INTSTATUS(base),
                CMSDK_UART_INT_TX | CMSDK_UART_INT_RX
                | CMSDK_UART_INT_TXOVRRUN | CMSDK_UART_INT_RXOVRRUN);

    /* ボーレート分周比（QEMU では値は意味を持たないが最小値16以上に） */
    sil_wrw_mem((void *) CMSDK_UART_BAUDDIV(base), 16U);

    /* 送受信を許可（割込みはまだ許可しない） */
    sil_wrw_mem((void *) CMSDK_UART_CTRL(base),
                CMSDK_UART_CTRL_TXEN | CMSDK_UART_CTRL_RXEN);
}

/*
 * シリアルI/Oポートのクローズ
 */
void eSIOPort_close(CELLIDX idx)
{
    CELLCB *p_cellcb = GET_CELLCB(idx);
    sil_wrw_mem((void *) CMSDK_UART_CTRL(ATTR_base), 0U);
}

/*
 * シリアルI/Oポートへの文字送信
 */
bool_t eSIOPort_putChar(CELLIDX idx, char c)
{
    CELLCB *p_cellcb = GET_CELLCB(idx);
    /* 送信バッファに空きがあるか（TXFULL==0） */
    if ((sil_rew_mem((void *) CMSDK_UART_STATE(ATTR_base))
                                & CMSDK_UART_STATE_TXFULL) == 0U) {
        sil_wrw_mem((void *) CMSDK_UART_DATA(ATTR_base), (uint32_t)(uint8_t) c);
        return true;
    }
    return false;
}

/*
 * シリアルI/Oポートからの文字受信
 */
int_t eSIOPort_getChar(CELLIDX idx)
{
    CELLCB *p_cellcb = GET_CELLCB(idx);
    /* 受信バッファに文字があるか（RXFULL==1） */
    if ((sil_rew_mem((void *) CMSDK_UART_STATE(ATTR_base))
                                & CMSDK_UART_STATE_RXFULL) != 0U) {
        /* DATA の読出しにより RXFULL はクリアされる */
        return (int_t)(uint8_t)(sil_rew_mem((void *) CMSDK_UART_DATA(ATTR_base)) & 0xffU);
    }
    return -1;
}

/*
 * シリアルI/Oポートからのコールバックの許可
 */
void eSIOPort_enableCBR(CELLIDX idx, uint_t cbrtn)
{
    CELLCB *p_cellcb = GET_CELLCB(idx);
    uint32_t base = ATTR_base;
    uint32_t ctrl = sil_rew_mem((void *) CMSDK_UART_CTRL(base));

    switch (cbrtn) {
        case SIOSendReady:
            ctrl |= CMSDK_UART_CTRL_TXINTEN;
            break;
        case SIOReceiveReady:
            ctrl |= CMSDK_UART_CTRL_RXINTEN;
            break;
    }
    sil_wrw_mem((void *) CMSDK_UART_CTRL(base), ctrl);
}

/*
 * シリアルI/Oポートからのコールバックの禁止
 */
void eSIOPort_disableCBR(CELLIDX idx, uint_t cbrtn)
{
    CELLCB *p_cellcb = GET_CELLCB(idx);
    uint32_t base = ATTR_base;
    uint32_t ctrl = sil_rew_mem((void *) CMSDK_UART_CTRL(base));

    switch (cbrtn) {
        case SIOSendReady:
            ctrl &= ~CMSDK_UART_CTRL_TXINTEN;
            break;
        case SIOReceiveReady:
            ctrl &= ~CMSDK_UART_CTRL_RXINTEN;
            break;
    }
    sil_wrw_mem((void *) CMSDK_UART_CTRL(base), ctrl);
}

/*
 * シリアルI/Oポートに対する割込み処理
 */
void eiISR_main(CELLIDX idx)
{
    CELLCB *p_cellcb = GET_CELLCB(idx);
    uint32_t base = ATTR_base;
    uint32_t stat = sil_rew_mem((void *) CMSDK_UART_INTSTATUS(base));

    if ((stat & CMSDK_UART_INT_TX) != 0U) {
        sil_wrw_mem((void *) CMSDK_UART_INTSTATUS(base), CMSDK_UART_INT_TX);
        ciSIOCBR_readySend();
    }
    if ((stat & CMSDK_UART_INT_RX) != 0U) {
        sil_wrw_mem((void *) CMSDK_UART_INTSTATUS(base), CMSDK_UART_INT_RX);
        ciSIOCBR_readyReceive();
    }
}
