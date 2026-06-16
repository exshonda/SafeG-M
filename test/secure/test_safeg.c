/*
 *  SafeG-M 遷移テスト: Secure 側テスト本体
 *
 *  Secure(ASP3) と Non-secure(ns_test_main) の遷移を AI 向け機械可読出力で
 *  検証する．制御は主に Non-secure からの gate 呼び出し(test_gate.c)と，
 *  Secure 側の周期ハンドラ/例外ハンドラが駆動する．
 *
 *  カテゴリ:
 *    A  Secure→NS 起動/復帰      (A1/A2 は NS 側, A3 は restart_task)
 *    B  NS→Secure gate           (gate 内で記録)
 *    C  NS 中の割込み/ディスパッチ (tick_handler→hi_task)
 *    D1 NS 中の CPU 例外捕捉       (cpuexc_handler→exc_task)
 */
#include <kernel.h>
#include <t_syslog.h>
#include "syssvc/syslog.h"
#include "core_insn.h"
#include "kernel_cfg.h"
#include "test_harness.h"
#include "test_safeg.h"

/*
 *  gate / ハンドラと共有する状態
 */
volatile uint32_t	g_phase;
volatile uint32_t	g_restart_req;
volatile uint32_t	g_ns_looping;
volatile uint32_t	g_c_done;
volatile uint32_t *	g_ns_flag;

static volatile uint32_t	g_tick;
static volatile uint32_t	g_wd_fired;

#define WD_TICKS	150u	/* 100ms * 150 = 15s でフェイルセーフ発火 */

/*
 *  メインタスク: ハーネス開始後，BTASK(NS) に主導権を渡すため待ち状態へ．
 */
void
main_task(EXINF exinf)
{
	test_start("safeg-trans");
	(void)slp_tsk();		/* 通常は起床しない（全テストは gate/handler 駆動） */
	(void)ext_tsk();
}

/*
 *  A3: BTASK 再起動タスク．tg_request_restart の wup_tsk で起床し，
 *  フェーズを進めて BTASK を停止→再起動する（NS が reset から再実行される）．
 */
void
restart_task(EXINF exinf)
{
	for (;;) {
		(void)slp_tsk();
		g_phase++;
		(void)ter_tsk(_SAFEG_BTASK);
		(void)act_tsk(_SAFEG_BTASK);
	}
}

/*
 *  C1/C2/C3: NS 実行を横取りして走る高優先度 Secure タスク．
 *  tick_handler により NS ループ中に act される．
 */
void
hi_task(EXINF exinf)
{
	T_RTSK		rtsk;
	uint32_t	bp;
	ER			ercd;

	check_point(CP('C', 1));					/* NS 実行を横取りできた   */

	ercd = ref_tsk(_SAFEG_BTASK, &rtsk);
	check_ercd(ercd, E_OK);
	tst_chk_u32(0xC2, (uint32_t)rtsk.tskstat, (uint32_t)TTS_RDY);
												/* 横取り中 BTASK は READY */
	bp = get_basepri();
	tst_chk_u32(0xC3, bp, TST_IIPM_ENAALL);		/* Secure タスクは NS マスク中 */

	g_c_done = 1;
	if (g_ns_flag != NULL) {
		*g_ns_flag = 1;							/* NS ループを抜けさせる(Secure→NS RAM 書込み) */
	}
	(void)ext_tsk();
}

/*
 *  D1: NS の CPU 例外を Secure で捕捉した後の後始末タスク．
 */
void
exc_task(EXINF exinf)
{
	tst_done();						/* D1 まで到達したので集計を出力 */
	(void)ter_tsk(_SAFEG_BTASK);	/* NS を停止して再 fault を防ぐ   */
	(void)slp_tsk();
}

/*
 *  周期ハンドラ(100ms): NS 横取りの誘発 ＋ ウォッチドッグ．
 */
void
tick_handler(EXINF exinf)
{
	g_tick++;

	/* C: NS がループ中なら高優先度タスクで横取りする */
	if (g_ns_looping != 0 && g_c_done == 0) {
		(void)act_tsk(HI_TASK);
	}

	/* フェイルセーフ: 一定時間 DONE に達しなければ強制終端 */
	if (tst_result.done == 0 && g_tick > WD_TICKS && g_wd_fired == 0) {
		g_wd_fired = 1;
		tst_mark(0xDEAD);
		tst_done();
		(void)ter_tsk(_SAFEG_BTASK);
	}
}

/*
 *  Secure UsageFault(exc6) ハンドラ．
 *  注意: SafeG-M 有効時，exc6 のベクタは _kernel_usagefault_handler に
 *  差し替えられ deactivate_nonsecure_interrupts に専有される．Secure で
 *  「意図しない」UsageFault が起きた場合のみ core_exc_entry 経由でここへ来る．
 *  NS 由来の CPU 例外はここではなく hardfault_handler(exc3) で捕捉する．
 */
void
cpuexc_handler(void *p_excinf)
{
	tst_mark(0xE6E6u);					/* Secure UsageFault 観測用マーカ */
	(void)act_tsk(EXC_TASK);
}

/*
 *  D1: NS で発生した CPU 例外を Secure 側 HardFault(exc3) で捕捉する．
 *
 *  NS の `udf` は NS 側 UsageFault 未許可のため HardFault にエスカレーション
 *  し，AIRCR.BFHFNMINS=0 により Secure の HardFault(例外番号 3)として入る．
 *  p_excinf のレイアウト(core_support.S core_exc_entry 参照)は
 *    [0]=primask<<8|basepri, [1]=EXC_RETURN, [2..]=HW 例外フレーム
 *  EXC_RETURN.S(bit6)==0 なら例外が Non-secure 状態から入った＝NS 由来．
 *  sample1.c:248 の cpuexc_handler を雛形とする．
 */
void
hardfault_handler(void *p_excinf)
{
	volatile uint32_t *const SCB_CFSR = (volatile uint32_t *)0xE000ED28U;
	volatile uint32_t *const SCB_HFSR = (volatile uint32_t *)0xE000ED2CU;
	uint32_t	exc_return = ((const uint32_t *)p_excinf)[1];
	uint32_t	from_ns = ((exc_return & 0x40u) == 0u) ? 1u : 0u;	/* EXC_RETURN.S==0 */
	uint32_t	hfsr = *SCB_HFSR;

	check_point(CP('D', 1));					/* NS 例外を Secure(HardFault) が捕捉 */
	tst_chk_u32(0xD1, from_ns, 1u);				/* 捕捉した例外が NS 由来であること   */
	tst_mark(0xF000u | (hfsr >> 28));			/* 診断: HFSR 上位(FORCED=bit30 等)   */

	*SCB_CFSR = *SCB_CFSR;						/* W1C: Configurable Fault Status クリア */
	*SCB_HFSR = hfsr;							/* W1C: HardFault Status クリア          */

	(void)act_tsk(EXC_TASK);					/* 後始末タスクへ（exc_task が tst_done）*/
}
