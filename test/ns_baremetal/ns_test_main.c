/*
 *  SafeG-M 遷移テスト: Non-secure 側（最小ベアメタル）
 *
 *  FreeRTOS を用いず，遷移検証に必要な最小の Non-secure コードのみで構成する．
 *  すべての記録・判定は Secure 側 gate(test_gate.c)経由で行うため，本ファイルは
 *  gate を所定の順序で呼ぶだけでよい．ResetISR(startup.c)→main の順で起動する．
 *
 *  ビルドフラグ（EXTRA_CFLAGS で指定）:
 *    既定          : A1,A2,B1〜B4 を実行して DONE（実機 green を確認済み）
 *    -DTST_ENABLE_A3: A3(BTASK 再起動) を追加
 *    -DTST_ENABLE_C : C(NS 実行中の横取り/ディスパッチ) を追加
 *    -DTST_ENABLE_D1: D1(NS 中の CPU 例外捕捉) を追加
 *
 *  注意: A3 / C / D1 は「NS に関わる文脈切替（BTASK の ter/act, NS 実行の横取り,
 *  NS 例外）」を伴い，現状の SafeG-M ディスパッチャでは FPU コンテキスト退避
 *  (vstmdb {s16-s31}) で HardFault に至る．詳細は doc/test_safeg.md を参照．
 */
#include <stdint.h>
#include "test_gate.h"

int
main(void)
{
	uint32_t control, faultmask;

	/* A2: 起動時の保証状態を読む（CONTROL==0, FAULTMASK==1）       */
	/*     gate 呼び出しや割込み許可より前に取得すること           */
	__asm volatile ("mrs %0, control"   : "=r"(control));
	__asm volatile ("mrs %0, faultmask" : "=r"(faultmask));

#ifdef TST_ENABLE_A3
	if (tg_get_phase() == 0u) {
		/* ---- 初回起動: カテゴリ A (起動) ---- */
		tg_checkpoint(CP('A', 1));					/* A1: NS が起動・到達 */
		tg_chk_u32(TGCHK_CONTROL_NS, control, 0u);	/* A2: CONTROL==0      */
		tg_chk_u32(TGCHK_FAULTMASK,  faultmask, 1u);/* A2: FAULTMASK==1    */
		tg_request_restart();						/* A3 のため再起動要求 */
		for (;;) {
			/* restart_task に停止・再起動されるのを待つ */
		}
	}
	tg_checkpoint(CP('A', 3));						/* A3: 再起動を確認 */
#else
	/* ---- カテゴリ A1/A2（再起動なし） ---- */
	tg_checkpoint(CP('A', 1));						/* A1: NS が起動・到達 */
	tg_chk_u32(TGCHK_CONTROL_NS, control, 0u);		/* A2: CONTROL==0      */
	tg_chk_u32(TGCHK_FAULTMASK,  faultmask, 1u);	/* A2: FAULTMASK==1    */
#endif

	/* ---- カテゴリ B: NS→Secure gate ---- */
	tg_checkpoint(CP('B', 1));						/* B1: gate 呼出し成立 */
	tg_api_check();									/* B2: gate 内 API E_OK */
	tg_basepri_check();								/* B3: BASEPRI_S 復帰   */
	tg_mark(0xB4u);									/* B4: 多重呼出し       */
	tg_checkpoint(CP('B', 4));
	tg_mark(0xB4u);

#ifdef TST_ENABLE_C
	/* ---- カテゴリ C: NS 実行中の割込み/ディスパッチ ---- */
	/* NS 状態のままループし，Secure タイマ割込み→ディスパッチ→hi_task に */
	/* 横取りされるのを待つ．hi_task が ns_c_flag=1 を書いたら抜ける．     */
	tg_mark(0xC0u);
	{
		static volatile uint32_t ns_c_flag;
		volatile uint32_t i;

		ns_c_flag = 0;
		tg_begin_preempt(&ns_c_flag);				/* C1/C2/C3 は hi_task が記録 */
		for (i = 0u; i < 300000000u && ns_c_flag == 0u; i++) {
			/* NS 状態で待機（Secure 横取りされる） */
		}
		tg_end_preempt();
	}
#endif

#ifdef TST_ENABLE_D1
	/* ---- カテゴリ D1: NS 中の CPU 例外 → Secure 捕捉 ---- */
	tg_checkpoint(CP('D', 0));						/* これから例外を起こす */
	__asm volatile ("udf #0");						/* Secure cpuexc_handler が捕捉・終端 */
#else
	tg_finish();									/* SUMMARY + DONE */
#endif

	for (;;) {
	}
	return 0;
}
