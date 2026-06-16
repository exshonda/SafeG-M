/*
 *  SafeG-M 遷移テスト: Non-secure 側（FreeRTOS 版）
 *
 *  Non-secure を FreeRTOS で動かし，FreeRTOS タスクから Secure gate を叩いて
 *  A/B カテゴリを検証する統合版．A2(起動時保証状態)は FreeRTOS がスケジューラ
 *  起動時に CONTROL/スタックを変更するため，main() のスケジューラ起動前
 *  （= NS 起動直後の状態）で読み取る．
 */
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>
#include "test_gate.h"

/* A2: NS 起動直後に読み取った保証状態（main で取得） */
static uint32_t g_ns_control;
static uint32_t g_ns_faultmask;

static void
test_task(void *arg)
{
	(void)arg;

	/* ---- カテゴリ A1/A2 ---- */
	tg_checkpoint(CP('A', 1));							/* A1: FreeRTOS タスクから到達 */
	tg_chk_u32(TGCHK_CONTROL_NS, g_ns_control, 0u);		/* A2: 起動時 CONTROL==0 */
	tg_chk_u32(TGCHK_FAULTMASK,  g_ns_faultmask, 1u);	/* A2: 起動時 FAULTMASK==1 */

	/* ---- カテゴリ B ---- */
	tg_checkpoint(CP('B', 1));							/* B1: gate 呼出し成立 */
	tg_api_check();										/* B2: gate 内 API E_OK */
	tg_basepri_check();									/* B3: BASEPRI_S 復帰   */
	tg_mark(0xB4u);										/* B4: 多重呼出し       */
	tg_checkpoint(CP('B', 4));
	tg_mark(0xB4u);

	tg_finish();										/* SUMMARY + DONE */

	for (;;) {
		vTaskDelay(1000);
	}
}

int
main(void)
{
	/* NS 起動直後の保証状態を取得（スケジューラ起動・割込み許可より前） */
	__asm volatile ("mrs %0, control"   : "=r"(g_ns_control));
	__asm volatile ("mrs %0, faultmask" : "=r"(g_ns_faultmask));

	*(volatile uint32_t *)0xE000E010 = 0;			/* SysTick 無効化（再起動対策） */
	*(volatile uint32_t *)0xE000ED04 = 1u << 25;	/* SysTick ペンディングクリア   */
	__asm volatile ("cpsie f");						/* 割込み許可                   */

	xTaskCreate(test_task, "tst", configMINIMAL_STACK_SIZE + 200,
				NULL, configMAX_PRIORITIES - 1, NULL);
	vTaskStartScheduler();
	return 0;
}
