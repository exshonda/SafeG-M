/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
 * 
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2005-2016 by Embedded and Real-Time Systems Laboratory
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
 * ターゲット依存モジュール（LPC55S69用）
 */
#include "kernel_impl.h"
#include "target_syssvc.h"
#include <sil.h>
#ifdef TOPPERS_OMIT_TECS
#include "chip_serial.h"
#endif

/*
 * エラー時の処理
 */
extern void Error_Handler(void);

/*
 * 起動時のハードウェア初期化処理
 */
void hardware_init_hook(void)
{
    /* Power up crystal oscillator */
    sil_wrw_mem(LPC5500_PMC_PDRUNCFGCLR0, LPC5500_PMC_PDRUNCFG0_PDEN_XTAL32M | LPC5500_PMC_PDRUNCFG0_PDEN_LDOXO32M);
    /* Enable CLKIN from crystal oscillator */
    sil_orw(LPC5500_SYSCON_CLOCK_CTRL, LPC5500_SYSCON_CLOCK_CTRL_CLKIN_ENA);
    /* Enable 16 MHz crystal oscillator */
    sil_orw(LPC5500_ANACTRL_XO32M_CTRL, LPC5500_ANACTRL_XO32M_CTRL_ENABLE_SYSTEM_CLK_OUT);
    while ((sil_rew_mem(LPC5500_ANACTRL_XO32M_STATUS) & LPC5500_ANACTRL_XO32M_STATUS_XO_READY) == 0) ; /* Wait to be stable */

    sil_wrw_mem(LPC5500_PMC_PDRUNCFGCLR0, LPC5500_PMC_PDRUNCFG0_PDEN_PLL0); /* Power up PLL0 */
    sil_wrw_mem(LPC5500_SYSCON_PLL0CLKSEL, LPC5500_SYSCON_PLL0CLKSEL_SEL_CLKIN); /* Select CLKIN as PLL0 input clock */

    /*
     * PLL block diagram
     *
     *   Fin    +----+   Fref    +----+   Fcco    +-----+
     * -------> | /N | --------> | *M | --------> | /2P | ---> main_clk
     *          +----+           +----+           +-----+
     *
     * Where
     *     - Fin = 16 MHz
     *     - 275 MHz < Fcco < 550 MHz
     *     - 2 kHz < Fref < 150 MHz
     *
     * Parameters to obtain main_clk = 100 MHz
     *     N = 4
     *     Fref = Fin / N = 16 MHz / 4 = 4 MHz
     *     M = 100
     *     Fcco = Fref * M = 4 MHz * 100 = 400 MHz
     *     P = 2
     *     main_clk = Fcco / 2P = 400 MHz / (2 * 2) = 100 MHz
     */
    sil_wrw_mem(LPC5500_SYSCON_PLL0NDEC, LPC5500_SYSCON_PLL0NDEC_NDIV(4) | LPC5500_SYSCON_PLL0NDEC_NREQ);
    sil_wrw_mem(LPC5500_SYSCON_PLL0SSCG1,
                LPC5500_SYSCON_PLL0SSCG1_MDIV_EXT(100) | LPC5500_SYSCON_PLL0SSCG1_MREQ| LPC5500_SYSCON_PLL0SSCG1_SEL_EXT);
    sil_wrw_mem(LPC5500_SYSCON_PLL0PDEC, LPC5500_SYSCON_PLL0PDEC_PDIV(2) | LPC5500_SYSCON_PLL0PDEC_PREQ);
    sil_wrw_mem(LPC5500_SYSCON_PLL0CTRL,
                  LPC5500_SYSCON_PLL0CTRL_SELR(0)        /* selr = 0 */
                | LPC5500_SYSCON_PLL0CTRL_SELI(53)       /* seli = 2 * floor(M / 4) + 3 = 53 */
                | LPC5500_SYSCON_PLL0CTRL_SELP(26)       /* selp = floor(M / 4) + 1 = 26 */
                | LPC5500_SYSCON_PLL0CTRL_CLKEN);        /* Enable output */
    while ((sil_rew_mem(LPC5500_SYSCON_PLL0STAT) & LPC5500_SYSCON_PLL0STAT_LOCK) == 0) ; /* Wait to be locked */
    /* Flash access time up to 100 MHz */
    sil_wrw_mem(LPC5500_FLASH_INT_CLR_STATUS, LPC5500_FLASH_INT_STATUS_ALL);
    sil_wrw_mem(LPC5500_FLASH_DATAW(0), 8);
    sil_wrw_mem(LPC5500_FLASH_CMD, LPC5500_FLASH_CMD_SET_READ_MODE);
    while ((sil_rew_mem(LPC5500_FLASH_INT_STATUS) & LPC5500_FLASH_INT_STATUS_DONE) == 0) ;
    sil_wrw_mem(
        LPC5500_SYSCON_FMCCR,
        (sil_rew_mem(LPC5500_SYSCON_FMCCR) & ~(LPC5500_SYSCON_FMCCR_FLASHTIM_MASK)) | LPC5500_SYSCON_FMCCR_FLASHTIM(0x8)
    );
    sil_wrw_mem(LPC5500_SYSCON_AHBCLKDIV, LPC5500_SYSCON_AHBCLKDIV_DIV(0)); /* AHBCLK = main_clk / 1 */
    sil_wrw_mem(LPC5500_SYSCON_MAINCLKSELB, LPC5500_SYSCON_MAINCLKSELB_SEL_PLL0); /* Select PLL0 output as main clock */

    /* Serial port IOCON settings */
    sil_wrw_mem(LPC5500_SYSCON_AHBCLKCTRLSET0, LPC5500_SYSCON_AHBCLKCTRL0_IOCON); /* Enable IOCON clock */
    /* TX: P0_30(1), RX: P0_29(1) */
    sil_wrw_mem(LPC5500_IOCON_P0(29), LPC5500_IOCON_P_FUNC(1) | LPC5500_IOCON_P_DIGIMODE);
    sil_wrw_mem(LPC5500_IOCON_P0(30), LPC5500_IOCON_P_FUNC(1));
    sil_wrw_mem(LPC5500_SYSCON_AHBCLKCTRLCLR0, LPC5500_SYSCON_AHBCLKCTRL0_IOCON); /* Disable IOCON clock */
}

void software_init_hook(void)
{
    /* Initialize sio for fput */
#ifdef TOPPERS_OMIT_TECS
    sio_initialize(0);
    sio_opn_por(SIOPID_FPUT, 0);
#endif
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
void target_initialize(void)
{
    /*
     *  コア依存部の初期化
     */
    core_initialize();
    /*
     *  SIOを初期化
     */
#ifndef TOPPERS_OMIT_TECS
    tPutLogSIOPort_initialize();
#endif /* TOPPERS_OMIT_TECS */

#ifdef TOPPERS_SAFEG_M
    /* NSC region */
    sil_wrw_mem((uint32_t *)SAU_RNR, 0);
    sil_wrw_mem((uint32_t *)SAU_RBAR, 0x1004FE00);
    sil_wrw_mem((uint32_t *)SAU_RLAR, (0x1004FFFF & SAU_RLAR_LADDR_MASK) | SAU_RLAR_NSC | SAU_RLAR_ENABLE);
    /* NS region (Flash) */
    sil_wrw_mem((uint32_t *)SAU_RNR, 1);
    sil_wrw_mem((uint32_t *)SAU_RBAR, 0x50000);
    sil_wrw_mem((uint32_t *)SAU_RLAR, (0x9D7FF & SAU_RLAR_LADDR_MASK) | SAU_RLAR_ENABLE);
    /* NS region (RAM) */
    sil_wrw_mem((uint32_t *)SAU_RNR, 2);
    sil_wrw_mem((uint32_t *)SAU_RBAR, 0x20020000);
    sil_wrw_mem((uint32_t *)SAU_RLAR, (0x20043FFF & SAU_RLAR_LADDR_MASK) | SAU_RLAR_ENABLE);
    sil_wrw_mem((uint32_t *)SAU_CTRL, SAU_CTRL_ENABLE);
    Asm("isb");
    Asm("dsb");
#endif /* TOPPERS_SAFEG_M */
}

/*
 * ターゲット依存部 終了処理
 */
void target_exit(void)
{
    /* チップ依存部の終了処理 */
    core_terminate();
    while(1) ;
}

/*
 * エラー発生時の処理
 */
void Error_Handler(void)
{
    while (1) ;
}

/*
 *  デフォルトのsoftware_term_hook（weak定義）
 */
__attribute__((weak))
void software_term_hook(void)
{
}
