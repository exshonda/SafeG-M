/*
 *  SafeG-M 遷移テスト: Secure アプリ局所定義（cfg と各 .c が include）
 */
#include <kernel.h>
#include "target_test.h"

/*
 *  タスク優先度（数値が小さいほど高優先度）
 *    EXC     : CPU 例外後始末タスク（最優先）
 *    RESTART : BTASK 再起動タスク（A3）
 *    HI      : NS 横取り確認タスク（C1/C2/C3）
 *    MAIN    : セットアップ用メインタスク
 *    BTASK   : Non-secure（TMAX_TPRI 最低優先度，core_kernel.cfg が自動生成）
 */
#define EXC_PRIORITY		1
#define RESTART_PRIORITY	2
#define HI_PRIORITY			4		/* LogTask(3) より低く，BTASK より高い */
#define MAIN_PRIORITY		5

#define STK_SIZE			4096

/*
 *  NS 由来の CPU 例外を捕捉する Secure 側の例外番号．
 *  NS の UsageFault は NS 側で未許可のため HardFault(例外番号 3)にエスカレー
 *  ションし，AIRCR.BFHFNMINS=0 により Secure の HardFault として入る．
 *  SafeG-M は Secure UsageFault(exc6) を deactivate 機構に専有しているため，
 *  NS 由来の例外は exc3 経路で捕捉する．
 */
#define HARDFAULT_EXCNO		3

/*
 *  Secure 動作中に Non-secure 割込みをマスクする BASEPRI_S の値
 *  （core_kernel_impl.h の IIPM_ENAALL と一致．kernel 内部ヘッダへ依存しない
 *    よう本テストでは数値で持つ．）
 */
#define TST_IIPM_ENAALL		0x80u

#ifndef TOPPERS_MACRO_ONLY

#include <stdint.h>

/*
 *  タスク・ハンドラ
 */
extern void	main_task(EXINF exinf);
extern void	hi_task(EXINF exinf);
extern void	exc_task(EXINF exinf);
extern void	restart_task(EXINF exinf);
extern void	tick_handler(EXINF exinf);		/* 周期: 横取り誘発＋ウォッチドッグ */
extern void	cpuexc_handler(void *p_excinf);
extern void	hardfault_handler(void *p_excinf);	/* D1: NS 由来例外(exc3)を捕捉 */

/*
 *  ハーネス／gate と共有する状態
 */
extern volatile uint32_t	g_phase;		/* 0=初回, 1=再起動後 */
extern volatile uint32_t	g_restart_req;	/* 再起動要求         */
extern volatile uint32_t	g_ns_looping;	/* NS が横取り待ちループ中 */
extern volatile uint32_t	g_c_done;		/* C カテゴリ確認済み */
extern volatile uint32_t *	g_ns_flag;		/* 横取り成立を NS へ知らせるフラグ(NS RAM) */

#endif /* TOPPERS_MACRO_ONLY */
