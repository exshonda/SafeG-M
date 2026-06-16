/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
 * 
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2005-2020 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 * 
 *  上記著作権者は，以下の(1)～(4)の条件を満たす場合に限り，本ソフトウェ
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
 *      用できない形で再配布する場合には，次のいずれかの条件を満たすこ
 *      と．
 *    (a) 再配布に伴うドキュメント（利用者マニュアルなど）に，上記の著
 *        作権表示，この利用条件および下記の無保証規定を掲載すること．
 *    (b) 再配布の形態を，別に定める方法によって，TOPPERSプロジェクトに
 *        報告すること．
 *  (4) 本ソフトウェアの利用により直接的または間接的に生じるいかなる損
 *      害からも，上記著作権者およびTOPPERSプロジェクトを免責すること．
 *      また，本ソフトウェアのユーザまたはエンドユーザからのいかなる理
 *      由に基づく請求からも，上記著作権者およびTOPPERSプロジェクトを
 *      免責すること．
 * 
 *  本ソフトウェアは，無保証で提供されているものである．上記著作権者お
 *  よびTOPPERSプロジェクトは，本ソフトウェアに関して，特定の使用目的
 *  に対する適合性も含めて，いかなる保証も行わない．また，本ソフトウェ
 *  アの利用により直接的または間接的に生じたいかなる損害に関しても，そ
 *  の責任を負わない．
 * 
 */

/*
 * ターゲット依存モジュール（NUCLEO_L552ZEQ用）
 */
#include "kernel_impl.h"
#include <sil.h>
#include "stm32l5xx_ll_lpuart.h"

/*
 *  起動直後の初期化(system_stm32f4xx.c)
 */
extern void SystemInit(void);

/*
 *  クロックの初期化(systemclock_config.c)
 */
extern void SystemClock_Config(void);

/*
 *  エラー時の処理
 */
extern void Error_Handler(void);

/*
 *  起動時のハードウェア初期化処理
 */
void
hardware_init_hook(void) {
	/*
	 * BSPで用意された初期化処理
	 */
	SystemInit();

	/*
	 *  -fdata-sectionsを使用するとistkが削除され，
	 *  cfgのパス3のチェックがエラーとなるため，
	 *  削除されないようにする 
	 */
	SystemCoreClock = (uint32_t)istk;
}

#ifndef TOPPERS_OMIT_TECS
/*
 *  システムログの低レベル出力のための初期化
 *
 */
extern void tPutLogSIOPort_initialize(void);
#endif

/*
 * ターゲット依存部 初期化処理
 */
void
target_initialize(void)
{
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
	
	/*
	 *  クロックの初期化
	 */
	SystemClock_Config();

	/*
	 * キャッシュの有効化
	 */
	SET_BIT(ICACHE->CR, ICACHE_CR_EN);

	/*
	 *  コア依存部の初期化
	 */
	core_initialize();

	/*
	 *  使用するペリフェラルにクロックを供給
	 */

	/*
	 *  UserLEDの初期化
	 */
	nucleo_led_init();
	nucleo_led_off(NUCLEO_LED1);
	nucleo_led_off(NUCLEO_LED2);
	nucleo_led_off(NUCLEO_LED3);

	/*
	 *  SIOを初期化
	 */
	lpuart_low_init();
#ifndef TOPPERS_OMIT_TECS
	tPutLogSIOPort_initialize();
#endif /* TOPPERS_OMIT_TECS */

#ifdef TOPPERS_SAFEG_M
	/* Initialize SAU regions */
	SAU->RNR = 0;
	SAU->RBAR = 0x0C03E000 & SAU_RBAR_BADDR_Msk;
	SAU->RLAR = (0x0C03FFFF & SAU_RLAR_LADDR_Msk) | SAU_RLAR_NSC_Msk | SAU_RLAR_ENABLE_Msk;
	SAU->RNR = 1;
	SAU->RBAR = 0x08040000 & SAU_RBAR_BADDR_Msk;
	SAU->RLAR = (0x0807FFFF & SAU_RLAR_LADDR_Msk) | SAU_RLAR_ENABLE_Msk;
	SAU->RNR = 2;
	SAU->RBAR = 0x20018000 & SAU_RBAR_BADDR_Msk;
	SAU->RLAR = (0x2003FFFF & SAU_RLAR_LADDR_Msk) | SAU_RLAR_ENABLE_Msk;
	SAU->RNR = 3;
	SAU->RBAR = 0x40000000 & SAU_RBAR_BADDR_Msk;
	SAU->RLAR = (0x4FFFFFFF & SAU_RLAR_LADDR_Msk) | SAU_RLAR_ENABLE_Msk;
	SAU->RNR = 4;
	SAU->RBAR = 0x60000000 & SAU_RBAR_BADDR_Msk;
	SAU->RLAR = (0x9FFFFFFF & SAU_RLAR_LADDR_Msk) | SAU_RLAR_ENABLE_Msk;
	SAU->RNR = 5;
	SAU->RBAR = 0x0BF90000 & SAU_RBAR_BADDR_Msk;
	SAU->RLAR = (0x0BFA8FFF & SAU_RLAR_LADDR_Msk) | SAU_RLAR_ENABLE_Msk;
	SAU->CTRL = SAU_CTRL_ENABLE_Msk;
	__ISB();
	__DSB();
	/* Allow non-secure interrupts if needed */
	/* NVIC->ITNS[x] = y; */

	/* Enable GTZC clock */
	RCC->AHB1ENR |= RCC_AHB1ENR_GTZCEN;
	/* Wait 2 cycles */
	Asm ("mov r0, r0");
	Asm ("mov r0, r0");
	GTZC_TZIC->FCR1 = 0xFFFFFFFF;
	GTZC_TZIC->FCR2 = 0x3FFF6FFF;
	GTZC_TZIC->FCR3 = 0xFF;
	/* Enable GTZC_IRQn if needed */
	/* Set ICACHE secure */
	GTZC_TZSC->SECCFGR2 |= GTZC_CFGR2_ICACHE_REG_Msk;
	/* Set SRAM security attributes */
	for (int i = 0; i < 12; ++i) {
		GTZC_MPCBB1->VCTR[i] = 0xFFFFFFFF;
	}
	for (int i = 12; i < 24; ++i) {
		GTZC_MPCBB1->VCTR[i] = 0;
	}
	for (int i = 0; i < 8; ++i) {
		GTZC_MPCBB2->VCTR[i] = 0;
	}
#endif /* TOPPERS_SAFEG_M */
}

/*
 * ターゲット依存部 終了処理
 */
void
target_exit(void)
{
	/* チップ依存部の終了処理 */
	core_terminate();
	while(1);
}

/*
 * エラー発生時の処理
 */
void
Error_Handler(void){
	volatile int loop;
//	BSP_LED_Init(LED2);
	while(1){
		for(loop = 0; loop < 0x100000; loop++);
//		BSP_LED_Toggle(LED2);
	}
}

/*
 *  デフォルトのsoftware_term_hook（weak定義）
 */
__attribute__((weak))
void software_term_hook(void)
{
}
