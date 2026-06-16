/*
 *  SafeG-M 遷移テスト: Secure 側テスト gate 群
 *
 *  Non-secure から呼び出される cmse_nonsecure_entry 関数．sample1.c の
 *  nonsecure_task / nonsecure_message と同じパターンで，末尾に set_basepri(0)
 *  を置き Non-secure 割込みを再許可してから戻る（BASEPRI_S 方式の要請）．
 */
#include <kernel.h>
#include <t_syslog.h>
#include "syssvc/syslog.h"
#include "core_insn.h"
#include "kernel_cfg.h"
#include "test_harness.h"
#include "test_safeg.h"

#define NSE		__attribute__((cmse_nonsecure_entry))

/*
 *  チェックポイント記録
 */
void NSE
tg_checkpoint(uint32_t cp)
{
	check_point((uint_t)cp);
	set_basepri(0);
}

/*
 *  値検査
 */
void NSE
tg_chk_u32(uint32_t id, uint32_t actual, uint32_t expected)
{
	tst_chk_u32(id, actual, expected);
	set_basepri(0);
}

/*
 *  フリーマーカ
 */
void NSE
tg_mark(uint32_t tag)
{
	tst_mark(tag);
	set_basepri(0);
}

/*
 *  フェーズ取得（0=初回, 1=再起動後）
 */
uint32_t NSE
tg_get_phase(void)
{
	uint32_t p = g_phase;
	set_basepri(0);
	return p;
}

/*
 *  A3: BTASK の再起動を要求（restart_task を起床）
 */
void NSE
tg_request_restart(void)
{
	g_restart_req = 1;
	(void)wup_tsk(RESTART_TASK);
	set_basepri(0);
}

/*
 *  B2: gate 内で ASP3 API を発行し E_OK を確認
 */
void NSE
tg_api_check(void)
{
	ID	tid;
	ER	ercd;

	ercd = get_tid(&tid);
	tst_chk_u32(TGCHK_API_ERCD, (uint32_t)ercd, (uint32_t)E_OK);
	set_basepri(0);
}

/*
 *  B3: BASEPRI_S 復帰機構の検査
 *
 *  API が BASEPRI_S を 0x80 (IIPM_ENAALL) に残したまま戻る状況を模擬し，
 *  set_basepri(0) によって確実に 0 へ戻る（= Non-secure 割込みが再許可される）
 *  ことを検査する．これが SafeG-M の BASEPRI_S 方式の肝である．
 */
void NSE
tg_basepri_check(void)
{
	uint32_t entry, after;

	set_basepri(TST_IIPM_ENAALL);
	entry = get_basepri();
	tst_chk_u32(TGCHK_BASEPRI_ENTRY, entry, TST_IIPM_ENAALL);

	set_basepri(0);
	after = get_basepri();
	tst_chk_u32(TGCHK_BASEPRI_AFTER, after, 0);
	/* 末尾は BASEPRI=0（NS 割込み許可）のまま戻る */
}

/*
 *  C1/C2/C3 開始: NS が NS 状態のままループして Secure タイマに横取りされる
 *  のを待つための準備．横取りした hi_task が *ns_flag=1 を書き込む．
 */
void NSE
tg_begin_preempt(volatile uint32_t *ns_flag)
{
	g_ns_flag = ns_flag;
	g_c_done = 0;
	g_ns_looping = 1;
	set_basepri(0);
}

/*
 *  C 終了: NS ループ中に横取り(hi_task)が成立したことを確認する．
 */
void NSE
tg_end_preempt(void)
{
	g_ns_looping = 0;
	tst_chk_u32(0xCEu, g_c_done, 1u);	/* 横取り・ディスパッチが起きた */
	set_basepri(0);
}

/*
 *  集計と終端センチネルの出力
 */
void NSE
tg_finish(void)
{
	/*
	 *  DONE 後は周期ハンドラを止め，NS(BTASK)からの不要なディスパッチ
	 *  (= NS の FPU コンテキスト退避を伴う文脈切替)が起きないようにする．
	 *  これを行わないと DONE 出力後に pendsv の vstmdb {s16-s31} で HardFault
	 *  に至る（NS 文脈切替時の既知の課題．doc/test_safeg.md 参照）．
	 */
	(void)stp_cyc(TICK_CYC);
	tst_done();
	set_basepri(0);
}
