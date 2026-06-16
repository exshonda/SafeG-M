/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
 *
 *  Copyright (C) 2026 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，以下の(1)〜(4)の条件を満たす場合に限り，本ソフトウェ
 *  ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
 *  変・再配布（以下，利用と呼ぶ）することを無償で許諾する．
 *  (1) 本ソフトウェアをソースコードの形で利用する場合には，上記の著作
 *      権表示，この利用条件および下記の無保証規定が，そのままの形でソー
 *      スコード中に含まれていること．
 *  (2) 本ソフトウェアを，ライブラリ形式など，他のソフトウェア開発に使
 *      用できる形で再配布する場合には，再配布に伴うドキュメント（利用
 *      者マニュアルなど）に，上記の著作権表示，この利用条件および下記
 *      の無保証規定を掲載すること．
 *  (3) 本ソフトウェアを，機器に組み込むなど，他のソフトウェア開発に使
 *      用できない形で再配布する場合には，次のいずれかの条件を満たすこと．
 *    (a) 再配布に伴うドキュメントに，上記の著作権表示，この利用条件およ
 *        び下記の無保証規定を掲載すること．
 *    (b) 再配布の形態を，別に定める方法によって，TOPPERSプロジェクトに
 *        報告すること．
 *  (4) 本ソフトウェアの利用により直接的または間接的に生じるいかなる損
 *      害からも，上記著作権者およびTOPPERSプロジェクトを免責すること．
 *      また，本ソフトウェアのユーザまたはエンドユーザからのいかなる理
 *      由に基づく請求からも，上記著作権者およびTOPPERSプロジェクトを免
 *      責すること．
 *
 *  本ソフトウェアは，無保証で提供されているものである．上記著作権者お
 *  よびTOPPERSプロジェクトは，本ソフトウェアに関して，特定の使用目的に
 *  対する適合性も含めて，いかなる保証も行わない．また，本ソフトウェア
 *  の利用により直接的または間接的に生じたいかなる損害に関しても，その
 *  責任を負わない．
 *
 */

/*
 *  ARM MPS2+ (AN505 / IoTKit, シングル Cortex-M33) サポートモジュール
 *
 *  本ファイルは，QEMU の "mps2-an505" マシンを対象とする．ハードウェア
 *  資源のアドレス・割込み番号は QEMU の hw/arm/mps2-tz.c および
 *  hw/arm/armsse.c に基づく．
 */

#ifndef TOPPERS_MPS2_AN505_H
#define TOPPERS_MPS2_AN505_H

/*
 *  コアのクロック周波数
 *
 *  QEMU の AN505 マシンの sysclk は 20MHz（mps2-tz.c の
 *  mmc->sysclk_frq = 20 * 1000 * 1000）．SysTick はプロセッサクロックで
 *  駆動され，APB ペリフェラル（UART）のクロックも同一である．
 */
#define CPU_CLOCK_HZ    20000000

/*
 *  割込み番号の最大値
 *
 *  SSE-200 の NVIC 外部割込みは 0〜123（内部 32 + 拡張 92）．例外番号は
 *  これに 16 を加えたもの．
 */
#define TMAX_INTNO      (124 + 16)

/*
 *  微少時間待ち（sil_dly_nse）のための定義（本来は SIL のターゲット依存部）
 *
 *  QEMU はサイクル精度のエミュレーションを行わないため，値は目安である．
 */
#define SIL_DLY_TIM1    10
#define SIL_DLY_TIM2    10

/*
 *  CMSDK APB UART
 *
 *  AN505 では UART0 のセキュアエイリアスが 0x40200000 に配置される．
 *  割込みは rx / tx / combined の 3 本があり（mps2-tz.c の make_uart 参照），
 *  combined（任意の要因で発火）を SIO ドライバの割込みに用いる．
 */
#define MPS2_AN505_UART0_BASE           0x40200000
#define MPS2_AN505_UART1_BASE           0x40201000
#define MPS2_AN505_UART2_BASE           0x40202000

#define MPS2_AN505_UART0_RX_IRQn        32
#define MPS2_AN505_UART0_TX_IRQn        33
#define MPS2_AN505_UART0_COMBINED_IRQn  42
#define MPS2_AN505_UART1_RX_IRQn        34
#define MPS2_AN505_UART1_TX_IRQn        35
#define MPS2_AN505_UART1_COMBINED_IRQn  43

/*
 *  CMSDK APB UART レジスタ（ベースアドレスからのオフセット）
 */
#define CMSDK_UART_DATA(base)       ((uint32_t)(base) + 0x00U)  /* データ */
#define CMSDK_UART_STATE(base)      ((uint32_t)(base) + 0x04U)  /* ステータス */
#define CMSDK_UART_CTRL(base)       ((uint32_t)(base) + 0x08U)  /* 制御 */
#define CMSDK_UART_INTSTATUS(base)  ((uint32_t)(base) + 0x0CU)  /* 割込みステータス／クリア */
#define CMSDK_UART_BAUDDIV(base)    ((uint32_t)(base) + 0x10U)  /* ボーレート分周比 */

/*
 *  STATE レジスタのビット
 */
#define CMSDK_UART_STATE_TXFULL     0x01U   /* 送信バッファフル */
#define CMSDK_UART_STATE_RXFULL     0x02U   /* 受信バッファフル */
#define CMSDK_UART_STATE_TXOVRRUN   0x04U   /* 送信オーバラン */
#define CMSDK_UART_STATE_RXOVRRUN   0x08U   /* 受信オーバラン */

/*
 *  CTRL レジスタのビット
 */
#define CMSDK_UART_CTRL_TXEN        0x01U   /* 送信許可 */
#define CMSDK_UART_CTRL_RXEN        0x02U   /* 受信許可 */
#define CMSDK_UART_CTRL_TXINTEN     0x04U   /* 送信割込み許可 */
#define CMSDK_UART_CTRL_RXINTEN     0x08U   /* 受信割込み許可 */

/*
 *  INTSTATUS／INTCLEAR レジスタのビット（書込み 1 でクリア）
 */
#define CMSDK_UART_INT_TX           0x01U   /* 送信割込み */
#define CMSDK_UART_INT_RX           0x02U   /* 受信割込み */
#define CMSDK_UART_INT_TXOVRRUN     0x04U   /* 送信オーバラン割込み */
#define CMSDK_UART_INT_RXOVRRUN     0x08U   /* 受信オーバラン割込み */

#endif /* TOPPERS_MPS2_AN505_H */
