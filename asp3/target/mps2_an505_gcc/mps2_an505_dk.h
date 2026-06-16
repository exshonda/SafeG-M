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
 *  ボード依存定義（ARM MPS2-AN505 / QEMU 用）
 *
 *  レジスタ・割込み番号・クロックの素の定義は mps2_an505.h にある．本ファ
 *  イルは SIO ポートの選択など「ボード」レベルの定義のみを与える．
 */

#ifndef TOPPERS_MPS2_AN505_DK_H
#define TOPPERS_MPS2_AN505_DK_H

#include "mps2_an505.h"

/*
 *  低レベル出力に使う SIO ポート
 *
 *  UART0（セキュアエイリアス 0x40200000）を使用する．割込みは combined
 *  （TX/RX いずれの要因でも発火）を SIO ドライバの割込みに用いる．
 *  SIOPORT*_IRQ は外部割込み番号（例外番号 = +16 は cdl 側で加算）．
 */
#define SIOPORT1_BASE   MPS2_AN505_UART0_BASE
#define SIOPORT1_IRQ    MPS2_AN505_UART0_COMBINED_IRQn

#endif /* TOPPERS_MPS2_AN505_DK_H */
