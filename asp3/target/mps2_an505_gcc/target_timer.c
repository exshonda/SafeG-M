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
 *  タイマドライバ（ARM MPS2-AN505 用，SysTick によるイベント駆動 HRT）
 *
 *  SysTick は 24bit ダウンカウンタで，区間の終わり（0 への到達）で割込みを
 *  発生して自動的に RVR にリロードする．本ドライバは次の方式で HRT を実現
 *  する．
 *
 *    hrt_base    現在の区間の開始時刻 [us]（単調増加）
 *    hrt_reload  現在の区間長 [tick]（= SYST_RVR + 1，常に HRT_CLOCKS_PER_US の倍数）
 *
 *    get_current = hrt_base + （区間内の経過 tick） / HRT_CLOCKS_PER_US
 *
 *  区間内の経過は (RVR - SYST_CVR) で求める．区間がまるごと 1 回経過したこと
 *  は「割込みが発生した」ことそのものが表すため，割込みハンドラで hrt_base に
 *  1 区間分を加える（ダウンカウンタが 1 周して元に戻るため端点だけの観測では
 *  経過が見えない問題を回避する）．
 *
 *  本実装は TOPPERS/ASP3CORE の mps2_an505_gcc/target_timer.c を SafeG-M の
 *  arm_m_gcc 移植層向けに移植したものである（QEMU の SysTick CVR 挙動に対応）．
 */

#include "kernel_impl.h"
#include "time_event.h"
#include "target_timer.h"
#include <sil.h>

/*
 *  現在の区間の開始時刻 [us]（単調増加）
 */
static volatile HRTCNT   hrt_base;

/*
 *  現在の区間長 [tick]（= SYST_RVR + 1）
 */
static volatile uint32_t hrt_reload;

/*
 *  前回 get_current が返した値（単調性保証のための下限）
 */
static volatile HRTCNT   hrt_last;

/*
 *  区間設定直後（SYST_CVR を 0 にクリアし，まだ RVR にリロードしていない）
 *  かどうか．QEMU では CVR 書込み直後にカウンタが 0 を返すため，この間の
 *  経過を 0 として扱い，区間長分の見かけの経過（スパイク）を防ぐ．
 */
static volatile uint8_t  hrt_fresh;

/*
 *  SysTick の制御レジスタ設定値（プロセッサクロック，割込み許可，動作）
 */
#define HRT_CTRL_RUN    (SYSTIC_ENABLE | SYSTIC_CLKSOURCE | SYSTIC_TICINT)

/*
 *  raise_event で用いる最小区間 [tick]（≒ 1us）
 */
#define HRT_MIN_TICKS   HRT_CLOCKS_PER_US

/*
 *  区間内の経過 tick 数を求める
 */
static uint32_t
hrt_offset_ticks(void)
{
    uint32_t reload = hrt_reload;
    uint32_t cur;
    uint32_t off;

    /* 割込み保留中（区間終端に到達済）なら 1 区間分が経過している */
    if ((sil_rew_mem((void *) NVIC_ICSR) & NVIC_PENDSTSET) != 0U) {
        return reload;
    }

    cur = sil_rew_mem((void *) SYSTIC_CURRENT_VALUE);

    if (hrt_fresh != 0U) {
        if (cur == 0U) {
            return 0U;          /* まだ RVR にリロードしていない */
        }
        hrt_fresh = 0U;         /* リロードされ，カウント開始した */
    }

    /* SYST_RVR = reload - 1 から cur まで減ったので経過は (reload-1) - cur */
    off = (reload - 1U) - cur;
    if (off > reload) {
        off = reload;           /* 念のためのクランプ */
    }
    return off;
}

/*
 *  SysTick を指定のリロード値（tick）で（再）起動する
 */
static void
hrt_program(uint32_t ticks)
{
    if (ticks < 2U) {
        ticks = 2U;
    }
    if (ticks > HRT_MAX_TICKS) {
        ticks = HRT_MAX_TICKS;
    }
    hrt_reload = ticks;
    hrt_fresh = 1U;

    sil_wrw_mem((void *) SYSTIC_CONTROL_STATUS, 0U);
    sil_wrw_mem((void *) SYSTIC_RELOAD_VALUE, ticks - 1U);
    sil_wrw_mem((void *) SYSTIC_CURRENT_VALUE, 0U);
    sil_wrw_mem((void *) NVIC_ICSR, NVIC_PENDSTCLR);
    /* COUNTFLAG をクリアするために一度読む */
    (void) sil_rew_mem((void *) SYSTIC_CONTROL_STATUS);
    sil_wrw_mem((void *) SYSTIC_CONTROL_STATUS, HRT_CTRL_RUN);
}

/*
 *  タイマの起動処理
 */
void
target_hrt_initialize(intptr_t exinf)
{
    hrt_base = 0U;
    hrt_last = 0U;
    /* 最初の set_event までは最大区間で空回しする */
    hrt_program(HRT_MAX_TICKS);
}

/*
 *  タイマの停止処理
 */
void
target_hrt_terminate(intptr_t exinf)
{
    sil_wrw_mem((void *) SYSTIC_CONTROL_STATUS, 0U);
    sil_wrw_mem((void *) NVIC_ICSR, NVIC_PENDSTCLR);
}

/*
 *  高分解能タイマの現在のカウント値の読出し（本体）
 */
HRTCNT
hrt_get_current_body(void)
{
    HRTCNT  v = (HRTCNT)(hrt_base + hrt_offset_ticks() / HRT_CLOCKS_PER_US);

    /*
     *  単調性の保証．区間境界での再設定やカウンタ読出しの僅かな前後により
     *  瞬間的に値が戻ることがある（時間が実際に戻るわけではない）．前回値
     *  より小さくなった場合は前回値を返す．2^32 us での正規の折返しと区別
     *  するため，差が範囲の半分以上のときだけ「戻った」と判定する．
     */
    if ((HRTCNT)(v - hrt_last) >= 0x80000000U) {
        v = hrt_last;
    }
    else {
        hrt_last = v;
    }
    return v;
}

/*
 *  高分解能タイマへの割込みタイミングの設定（本体）
 */
void
hrt_set_event_body(HRTCNT hrtcnt)
{
    /* これまでの経過を基準時刻に畳み込んでから区間を張り直す */
    hrt_base += hrt_offset_ticks() / HRT_CLOCKS_PER_US;

    if (hrtcnt == 0U) {
        hrtcnt = 1U;
    }
    hrt_program((uint32_t) hrtcnt * HRT_CLOCKS_PER_US);
}

/*
 *  高分解能タイマ割込みの要求（本体）
 *
 *  手動で割込み保留にすると「区間終端到達」と区別できないため，最小区間で
 *  張り直して直ちに（≒1us 後に）割込みを発生させる．
 */
void
hrt_raise_event_body(void)
{
    hrt_base += hrt_offset_ticks() / HRT_CLOCKS_PER_US;
    hrt_program(HRT_MIN_TICKS);
}

/*
 *  タイマ割込みハンドラ
 *
 *  区間終端に到達して発生した割込みなので，1 区間分を基準時刻に加える．
 *  カウンタは自動リロードされ次区間の計測を続けるが，signal_time() の最後で
 *  set_hrt_event() が区間を張り直す．
 */
void
target_hrt_handler(void)
{
    uint32_t csr = sil_rew_mem((void *) SYSTIC_CONTROL_STATUS);

    if ((csr & SYSTIC_COUNTFLAG) != 0U) {
        /* 区間終端に到達：1 区間分が経過した */
        hrt_base += hrt_reload / HRT_CLOCKS_PER_US;
        hrt_fresh = 0U;
    }
    else {
        /* 通常は起こらない（防御的に経過分だけ計上） */
        hrt_base += hrt_offset_ticks() / HRT_CLOCKS_PER_US;
    }
    sil_wrw_mem((void *) NVIC_ICSR, NVIC_PENDSTCLR);

    signal_time();
}
