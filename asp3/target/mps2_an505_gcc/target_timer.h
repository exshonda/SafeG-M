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
 * タイマドライバ（ARM MPS2-AN505 / QEMU 用）
 *
 *  KERNEL_TIMER=SYSTICK のときはプロセッサ依存部の core_timer.{c,h}（周期
 *  タイムティック）を用いる．既定（KERNEL_TIMER=TIM, USE_TIM_AS_HRT）では
 *  SysTick をイベント駆動の高分解能タイマ(HRT)として用いる（target_timer.c）．
 */

#ifndef TOPPERS_TARGET_TIMER_H
#define TOPPERS_TARGET_TIMER_H

#ifdef USE_SYSTICK_AS_TIMETICK

/*
 * プロセッサ依存部（周期タイムティック）で定義する
 */
#include "core_timer.h"

#else /* USE_SYSTICK_AS_TIMETICK */

#include "kernel/kernel_impl.h"
#include <sil.h>
#include "mps2_an505.h"

/*
 * タイマ割込みハンドラ登録のための定数（SysTick は例外番号 15）
 */
#define INTNO_TIMER  IRQNO_SYSTICK          /* 割込み番号 */
#define INHNO_TIMER  IRQNO_SYSTICK          /* 割込みハンドラ番号 */
#define INTPRI_TIMER (TMAX_INTPRI - 1)      /* 割込み優先度 */
#define INTATR_TIMER TA_NULL                /* 割込み属性 */

/*
 * プロセッサクロックの 1us あたりのカウント数（QEMU AN505 は 20MHz）
 */
#define HRT_CLOCKS_PER_US   (CPU_CLOCK_HZ / 1000000U)

/*
 * SysTick のリロード値の最大（24bit）
 */
#define HRT_MAX_TICKS       0x00FFFFFFU

/*
 * 割込みタイミングに指定する最大値（us）
 *
 * 24bit / 20MHz ＝ 約 0.84 秒．これより先のイベントはカーネルが分割する．
 */
#define HRTCNT_BOUND        (HRT_MAX_TICKS / HRT_CLOCKS_PER_US)

#ifndef TOPPERS_MACRO_ONLY

/*
 * 高分解能タイマの操作
 *
 *  target_hrt_get_current / set_event / raise_event は，システムサービスや
 *  カーネル本体が局所的に _kernel_ プレフィクスへリネームして参照するため，
 *  インライン関数として提供する（実体シンボルを作るとリネーム不整合でリンク
 *  できない）．本体ロジックと積算器の状態は target_timer.c に置き，ここでは
 *  薄い転送関数とする．
 */
extern void    target_hrt_initialize(intptr_t exinf);
extern void    target_hrt_terminate(intptr_t exinf);
extern void    target_hrt_handler(void);

extern HRTCNT  hrt_get_current_body(void);
extern void    hrt_set_event_body(HRTCNT hrtcnt);
extern void    hrt_raise_event_body(void);

Inline HRTCNT
target_hrt_get_current(void)
{
    return hrt_get_current_body();
}

Inline void
target_hrt_set_event(HRTCNT hrtcnt)
{
    hrt_set_event_body(hrtcnt);
}

Inline void
target_hrt_raise_event(void)
{
    hrt_raise_event_body();
}

#endif /* TOPPERS_MACRO_ONLY */

#endif /* USE_SYSTICK_AS_TIMETICK */

#endif /* TOPPERS_TARGET_TIMER_H */
