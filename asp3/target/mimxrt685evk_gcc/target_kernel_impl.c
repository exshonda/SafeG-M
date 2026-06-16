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
 * ターゲット依存モジュール（MIMXRT685-EVK用）
 */
#include "kernel_impl.h"
#include "target_syssvc.h"
#include <sil.h>
#ifdef TOPPERS_OMIT_TECS
#include "chip_serial.h"
#endif

#ifdef TOPPERS_SAFEG_M
#undef SCB_SCR
#endif
#define SCB_SCR           ((uint32_t *)0xE000ED10)
#define SCB_SCR_SLEEPDEEP (1 << 2)
#define PMC_PMIC_IRQn     58

/*
 * エラー時の処理
 */
extern void Error_Handler(void);

/*
 * Enter forward body-bias mode.
 * This function only works if it is executed on RAM, but the address is restricted (why?).
 * Load address 0x80000 works for example.
 */
static void __attribute__((noinline, section(".target_enter_fbb"))) target_enter_fbb(void)
{
    sil_wrw_mem((uint32_t *)(IMXRT600_PMC_BASE + 0x20), 0x04040808); /* undocumented register */
    sil_wrw_mem(IMXRT600_PMC_SLEEPCTRL, IMXRT600_PMC_SLEEPCTRL_0_7V);
    sil_orw(SCB_SCR, SCB_SCR_SLEEPDEEP);
    sil_wrw_mem(IMXRT600_SYSCON_SYSCTL0_PDSLEEPCFG0,
                (sil_rew_mem(IMXRT600_SYSCON_SYSCTL0_PDRUNCFG0)
                | IMXRT600_SYSCON_SYSCTL0_PDSLEEPCFG0_MAINCLK_SHUTOFF)
                & ~(IMXRT600_SYSCON_SYSCTL0_PDSLEEPCFG0_FBB_PD));
    sil_wrw_mem(IMXRT600_SYSCON_SYSCTL0_PDSLEEPCFG1, sil_rew_mem(IMXRT600_SYSCON_SYSCTL0_PDRUNCFG1));
    sil_wrw_mem(IMXRT600_SYSCON_SYSCTL0_PDSLEEPCFG2, sil_rew_mem(IMXRT600_SYSCON_SYSCTL0_PDRUNCFG2));
    sil_wrw_mem(IMXRT600_SYSCON_SYSCTL0_PDSLEEPCFG3, sil_rew_mem(IMXRT600_SYSCON_SYSCTL0_PDRUNCFG3));
    sil_wrw_mem(IMXRT600_SYSCON_SYSCTL0_PDWAKECFG, IMXRT600_SYSCON_SYSCTL0_PDWAKECFG_FBBKEEPST);
    sil_wrw_mem(IMXRT600_PMC_AUTOWKUP, 0x800);
    const uint32_t pmc_ctrl = sil_rew_mem(IMXRT600_PMC_CTRL);
    sil_wrw_mem(IMXRT600_PMC_CTRL,
                (pmc_ctrl | IMXRT600_PMC_CTRL_AUTOWKEN) & ~(IMXRT600_PMC_CTRL_LVDCOREIE | IMXRT600_PMC_CTRL_LVDCORERE));
    sil_wrw_mem(IMXRT600_SYSCON_SYSCTL0_STARTEN1_SET, IMXRT600_SYSCON_SYSCTL0_STARTEN1_PMIC);
    sil_wrw_mem((uint32_t *)(NVIC_SETENA0 + PMC_PMIC_IRQn / 32 * 4), 1 << (PMC_PMIC_IRQn % 32));
    /* Enter deep-sleep and wake up by PMIC interrupt */
    /* This sequence seems to put the MCU into FBB mode */
    Asm("wfi");
    sil_wrw_mem(IMXRT600_PMC_CTRL, pmc_ctrl);
    sil_wrw_mem(IMXRT600_SYSCON_SYSCTL0_STARTEN1_CLR, IMXRT600_SYSCON_SYSCTL0_STARTEN1_PMIC);
    sil_wrw_mem(IMXRT600_SYSCON_SYSCTL0_PDWAKECFG, 0);
    sil_wrw_mem((uint32_t *)(NVIC_CLRENA0 + PMC_PMIC_IRQn / 32 * 4), 1 << (PMC_PMIC_IRQn % 32));
    sil_andw(SCB_SCR, ~(SCB_SCR_SLEEPDEEP));
    while (sil_rew_mem(IMXRT600_PMC_STATUS) & IMXRT600_PMC_STATUS_ACTIVEFSM) ;
    /* Set core voltage 1.13V */
    sil_wrw_mem(IMXRT600_PMC_RUNCTRL, IMXRT600_PMC_RUNCTRL_1_13V);
    while (sil_rew_mem(IMXRT600_PMC_STATUS) & IMXRT600_PMC_STATUS_ACTIVEFSM) ;
    sil_orw(IMXRT600_PMC_CTRL, IMXRT600_PMC_CTRL_APPLYCFG);
    while (sil_rew_mem(IMXRT600_PMC_STATUS) & IMXRT600_PMC_STATUS_ACTIVEFSM) ;
}

static void pll_wait_half_of_lock(void)
{
    uint32_t count = sil_rew_mem(IMXRT600_SYSCON_CLKCTL0_SYSPLL0LOCKTIMEDIV2);
    uint32_t current = sil_rew_mem(IMXRT600_OSTIMER0_EVTIMERL);
    while (count) {
        const uint32_t new = sil_rew_mem(IMXRT600_OSTIMER0_EVTIMERL);
        if (current != new) {
            count -= 1;
            current = new;
        }
    }
}

extern const uint32_t __target_enter_fbb_end; /* Defined in linker script */

/*
 * 起動時のハードウェア初期化処理
 */
void hardware_init_hook(void)
{
    /* Load target_enter_fbb onto RAM */
    /* This operation is safe because it is before the initialization of RAM */
    const uint32_t *src = (uint32_t *)((uint32_t)target_enter_fbb & ~(1));
    uint32_t *dst = (uint32_t *)0x20080000; /* Data bus address */
    while (src < &__target_enter_fbb_end) {
        *dst = *src;
        src += 1;
        dst += 1;
    }
    Asm("cpsid i");
    Asm("cpsie f");
    ((void (*)(void))0x80001)(); /* Branch to code bus address */
    Asm("cpsid f");
    Asm("cpsie i");

    /* Initialize the PMIC I2C pina */
    sil_wrw_mem(IMXRT600_IOCON_FC15_I2C_SCL,
                IMXRT600_IOCON_P_IBENA | IMXRT600_IOCON_P_SLEWRATE | IMXRT600_IOCON_P_ODENA);
    sil_wrw_mem(IMXRT600_IOCON_FC15_I2C_SDA,
                IMXRT600_IOCON_P_IBENA | IMXRT600_IOCON_P_SLEWRATE | IMXRT600_IOCON_P_ODENA);

    /* main_clk -> Flexcomm 15 FRG -> Flexcomm 15 */
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL1_FRGnCLKSEL(15), IMXRT600_SYSCON_CLKCTL1_FRGnCLKSEL_SEL_MAIN_CLOCK);
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL1_FRGnCLKCTL(15),
                IMXRT600_SYSCON_CLKCTL1_FRGnCLKCTL_DIV(0xFF) | IMXRT600_SYSCON_CLKCTL1_FRGnCLKCTL_MULT(0));
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL1_FCnFCLKSEL(15), IMXRT600_SYSCON_CLKCTL1_FCnFCLKSEL_SEL_FCn_FRG_CLOCK);
    /* Enable Flexcomm 15 clock */
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL1_PSCCTL0_SET, IMXRT600_SYSCON_CLKCTL1_PSCCTL0_FCn_CLK(15));
    /* Clear Flexcomm 15 reset */
    sil_wrw_mem(IMXRT600_SYSCON_RSTCTL1_PRSTCTL0_CLR, IMXRT600_SYSCON_RSTCTL1_PRSTCTL0_FLEXCOMMn_RST(15));
    /* Select I2C function */
    sil_wrw_mem(IMXRT600_FLEXCOMM_PSELID(IMXRT600_FLEXCOMM15_BASE), IMXRT600_FLEXCOMM_PSELID_PERSEL_I2C);

    sil_wrw_mem(IMXRT600_I2C_CLKDIV(IMXRT600_I2C15_BASE), 48000000 / 10000 - 1); /* 10kHz */
    sil_wrw_mem(IMXRT600_I2C_MSTTIME(IMXRT600_I2C15_BASE), 0);
    sil_wrw_mem(IMXRT600_I2C_CFG(IMXRT600_I2C15_BASE), IMXRT600_I2C_CFG_MSTEN); /* Enable master */

    sil_wrw_mem(IMXRT600_PMC_CTRL, 0); /* Disable low-voltage reset */

    /* Set SW1 voltage of the PMIC 1.15V */
    sil_wrw_mem(IMXRT600_I2C_MSTDAT(IMXRT600_I2C15_BASE), 0b11000010); /* PCA9420 slave address */
    sil_wrw_mem(IMXRT600_I2C_MSTCTL(IMXRT600_I2C15_BASE), IMXRT600_I2C_MSTCTL_MSTSTART);
    while ((sil_rew_mem(IMXRT600_I2C_STAT(IMXRT600_I2C15_BASE)) & IMXRT600_I2C_STAT_MSTPENDING) == 0) ;
    sil_wrw_mem(IMXRT600_I2C_MSTDAT(IMXRT600_I2C15_BASE), 0x22); /* MODECFG_0_0 register */
    sil_wrw_mem(IMXRT600_I2C_MSTCTL(IMXRT600_I2C15_BASE), IMXRT600_I2C_MSTCTL_MSTCONTINUE);
    while ((sil_rew_mem(IMXRT600_I2C_STAT(IMXRT600_I2C15_BASE)) & IMXRT600_I2C_STAT_MSTPENDING) == 0) ;
    sil_wrw_mem(IMXRT600_I2C_MSTDAT(IMXRT600_I2C15_BASE), (1 << 6) | 0b011010); /* SW1 = 1.15V */
    sil_wrw_mem(IMXRT600_I2C_MSTCTL(IMXRT600_I2C15_BASE), IMXRT600_I2C_MSTCTL_MSTCONTINUE);
    while ((sil_rew_mem(IMXRT600_I2C_STAT(IMXRT600_I2C15_BASE)) & IMXRT600_I2C_STAT_MSTPENDING) == 0) ;
    sil_wrw_mem(IMXRT600_I2C_MSTCTL(IMXRT600_I2C15_BASE), IMXRT600_I2C_MSTCTL_MSTSTOP);
    while ((sil_rew_mem(IMXRT600_I2C_STAT(IMXRT600_I2C15_BASE)) & IMXRT600_I2C_STAT_MSTPENDING) == 0) ;

    /* Power down I2C15 */
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL1_PSCCTL0_CLR, IMXRT600_SYSCON_CLKCTL1_PSCCTL0_FCn_CLK(15));
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL1_FRGnCLKSEL(15), IMXRT600_SYSCON_CLKCTL1_FRGnCLKSEL_SEL_NONE);
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL1_FCnFCLKSEL(15), IMXRT600_SYSCON_CLKCTL1_FCnFCLKSEL_SEL_NONE);
    sil_wrw_mem(IMXRT600_IOCON_FC15_I2C_SCL, 0);
    sil_wrw_mem(IMXRT600_IOCON_FC15_I2C_SDA, 0);

    /* Power up OS Event Timer */
    sil_orw(IMXRT600_SYSCON_CLKCTL1_PSCCTL0_SET, IMXRT600_SYSCON_CLKCTL1_PSCCTL0_OSEVENT_TIMER_CLK);
    /*
     * Clock diagram
     *
     *       24MHz            +----------+  300MHz
     * XTAL ------> clk_in -> | Main PLL | -------> main_pll_clk -> main_clk
     *                        +----------+
     *
     * Main PLL settings
     * 24MHz -> * (20 + 5/6) -> 500MHz -> * 18/30 -> 300MHz
     */
    /* Power up crystal oscillator */
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL0_SYSOSCCTL0, IMXRT600_SYSCON_CLKCTL0_SYSOSCCTL0_LP_ENABLE);
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL0_SYSOSCBYPASS, 0);
    sil_wrw_mem(IMXRT600_SYSCON_SYSCTL0_PDRUNCFG0_CLR, IMXRT600_SYSCON_SYSCTL0_PDRUNCFG0_SYSXTAL_PD);
    /* Power down PLL */
    sil_wrw_mem(IMXRT600_SYSCON_SYSCTL0_PDRUNCFG0_SET,
                IMXRT600_SYSCON_SYSCTL0_PDRUNCFG0_SYSPLLLDO_PD | IMXRT600_SYSCON_SYSCTL0_PDRUNCFG0_SYSPLLANA_PD);
    /* Select crystal as PLL input */
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL0_SYSPLLCLKSEL, IMXRT600_SYSCON_CLKCTL0_SYSPLLCLKSEL_SEL_CLK_IN);
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL0_SYSPLL0NUM, 5);
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL0_SYSPLL0DENOM, 6);
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL0_SYSPLL0CTL0,
                IMXRT600_SYSCON_CLKCTL0_SYSPLL0CTL0_MULT(20) | IMXRT600_SYSCON_CLKCTL0_SYSPLL0CTL0_HOLDRINGOFF_ENA);
    /* Power up PLL */
    sil_wrw_mem(IMXRT600_SYSCON_SYSCTL0_PDRUNCFG0_CLR,
                IMXRT600_SYSCON_SYSCTL0_PDRUNCFG0_SYSPLLLDO_PD | IMXRT600_SYSCON_SYSCTL0_PDRUNCFG0_SYSPLLANA_PD);
    /* Wait for the half of the lock time */
    pll_wait_half_of_lock();
    sil_andw(IMXRT600_SYSCON_CLKCTL0_SYSPLL0CTL0, ~(IMXRT600_SYSCON_CLKCTL0_SYSPLL0CTL0_HOLDRINGOFF_ENA));
    pll_wait_half_of_lock();
    /* Power down OS Event Timer */
    sil_andw(IMXRT600_SYSCON_CLKCTL1_PSCCTL0_SET, ~(IMXRT600_SYSCON_CLKCTL1_PSCCTL0_OSEVENT_TIMER_CLK));
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL0_SYSPLL0PFD,
                IMXRT600_SYSCON_CLKCTL0_SYSPLL0PFD_PFD0(30) | IMXRT600_SYSCON_CLKCTL0_SYSPLL0PFD_PFD0_CLKGATE);
    sil_andw(IMXRT600_SYSCON_CLKCTL0_SYSPLL0PFD, ~(IMXRT600_SYSCON_CLKCTL0_SYSPLL0PFD_PFD0_CLKGATE));
    while ((sil_rew_mem(IMXRT600_SYSCON_CLKCTL0_SYSPLL0PFD) & IMXRT600_SYSCON_CLKCTL0_SYSPLL0PFD_PFD0_CLKRDY) == 0) ;
    sil_orw(IMXRT600_SYSCON_CLKCTL0_SYSPLL0PFD, IMXRT600_SYSCON_CLKCTL0_SYSPLL0PFD_PFD0_CLKRDY);
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL0_MAINPLLCLKDIV, 0);
    /* Switch main clock to Main PLL clock */
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL0_MAINCLKSELB, IMXRT600_SYSCON_CLKCTL0_MAINCLKSELB_SEL_MAIN_PLL_CLK);
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL0_SYSCPUAHBCLKDIV, 0);
    /* Switch FlexSPI clock Main clock / 3 */
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL0_FLEXSPIFCLKDIV, 3 - 1);
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL0_FLEXSPIFCLKSEL, IMXRT600_SYSCON_CLKCTL0_FLEXSPIFCLKSEL_SEL_MAIN_CLK);
    /* Set frg_pll 50MHz */
    sil_wrw_mem(IMXRT600_SYSCON_CLKCTL1_FRGPLLCLKDIV, CPU_CLOCK_HZ / FRG_PLL_HZ - 1);

    /* Clear all cache */
    sil_wrw_mem(IMXRT600_CACHE_CCR,
                IMXRT600_CACHE_CCR_INVW0 | IMXRT600_CACHE_CCR_PUSHW0
              | IMXRT600_CACHE_CCR_INVW1 | IMXRT600_CACHE_CCR_PUSHW1
              | IMXRT600_CACHE_CCR_GO);
    while ((sil_rew_mem(IMXRT600_CACHE_CCR) & IMXRT600_CACHE_CCR_GO) == 0) ;
    /*
     * FlexSPI external memory space is mapped on 0x0800_0000 - 0x1000_0000
     * Only this region of memory is cacheable
     *
     * FlexSPI cache interface handles boundary address as its internal address space
     * 0x1000_0000 - 0x0800_0000 = 0x0800_0000
     * Therefore, the range of boundary address is from 0x0000_0000 to 0x0800_0000
     */
    /* Set external memory space chachable (write-back) */
    sil_wrw_mem(IMXRT600_CACHE_REG0_TOP, 0x08000000 - 1);
    sil_wrw_mem(IMXRT600_CACHE_POLSEL, IMXRT600_CACHE_POLSEL_REG0_POLICY_WRITE_BACK);
    /* Enable cache */
    sil_wrw_mem(IMXRT600_CACHE_CCR, IMXRT600_CACHE_CCR_ENCACHE | IMXRT600_CACHE_CCR_ENWRBUF);
    /* Serial port IOCON settings */
    /* TX: P0_1(1), RX: P0_2(1) */
    sil_wrw_mem(IMXRT600_IOCON_P0(1), IMXRT600_IOCON_P_FUNC(1));
    sil_wrw_mem(IMXRT600_IOCON_P0(2), IMXRT600_IOCON_P_FUNC(1) | IMXRT600_IOCON_P_IBENA);
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
    sil_wrw_mem((uint32_t *)SAU_RBAR, 0x183FFE00);
    sil_wrw_mem((uint32_t *)SAU_RLAR, (0x183FFFFF & SAU_RLAR_LADDR_MASK) | SAU_RLAR_NSC | SAU_RLAR_ENABLE);
    /* NS region (Flash) */
    sil_wrw_mem((uint32_t *)SAU_RNR, 1);
    sil_wrw_mem((uint32_t *)SAU_RBAR, 0x8400000);
    sil_wrw_mem((uint32_t *)SAU_RLAR, (0x87FFFFF & SAU_RLAR_LADDR_MASK) | SAU_RLAR_ENABLE);
    /* NS region (RAM) */
    sil_wrw_mem((uint32_t *)SAU_RNR, 2);
    sil_wrw_mem((uint32_t *)SAU_RBAR, 0x20240000);
    sil_wrw_mem((uint32_t *)SAU_RLAR, (0x2047FFFF & SAU_RLAR_LADDR_MASK) | SAU_RLAR_ENABLE);
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
