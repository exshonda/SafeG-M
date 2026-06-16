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
 * ターゲット依存モジュール（NRF5340 DK用）
 */
#include "kernel_impl.h"
#include "target_syssvc.h"
#include <sil.h>
#ifdef TOPPERS_OMIT_TECS
#include "chip_serial.h"
#endif

#define XTAL_LOAD_PF 8

#ifdef TOPPERS_SAFEG_M
#define UART_BASE    NRF5340_UARTE1_BASE
#define UART_IRQn    NRF5340_UARTE1_IRQn
#define UART_TX_PORT 1
#define UART_TX_PIN  6
#define UART_RX_PORT 1
#define UART_RX_PIN  7
#else
#define UART_BASE    NRF5340_UARTE0_BASE
#define UART_IRQn    NRF5340_UARTE0_IRQn
#define UART_TX_PORT 0
#define UART_TX_PIN  20
#define UART_RX_PORT 0
#define UART_RX_PIN  22
#endif

#ifdef TOPPERS_SAFEG_M
#include "target_timer.h"

extern const uint32_t __flash_start;
extern const uint32_t __flash_end;
extern const uint32_t __nsc_start;
extern const uint32_t __nsc_size;
extern const uint32_t __ram_start;
extern const uint32_t __ram_end;
#endif /* TOPPERS_SAFEG_M */

/*
 * エラー時の処理
 */
extern void Error_Handler(void);

/*
 * 起動時のハードウェア初期化処理
 */
void hardware_init_hook(void)
{
    sil_wrw_mem(NRF5340_DEBUG_APPROTECT_DISABLE, sil_rew_mem(NRF5340_UICR_APPROTECT));
    sil_wrw_mem(NRF5340_DEBUG_SECUREAPPROTECT_DISABLE, sil_rew_mem(NRF5340_UICR_SECUREAPPROTECT));

    sil_wrw_mem(NRF5340_CACHE_MODE, NRF5340_CACHE_MODE_CACHE);
    sil_wrw_mem(NRF5340_CACHE_ENABLE, 1);

    const uint32_t slope = (sil_rew_mem(NRF5340_FICR_XOSC32MTRIM)
        & NRF5340_FICR_XOSC32MTRIM_SLOPE_MASK) >> NRF5340_FICR_XOSC32MTRIM_SLOPE_POS;
    const uint32_t offset = (sil_rew_mem(NRF5340_FICR_XOSC32MTRIM)
        & NRF5340_FICR_XOSC32MTRIM_OFFSET_MASK) >> NRF5340_FICR_XOSC32MTRIM_OFFSET_POS;
    const uint32_t cap = (((slope + 56)*(XTAL_LOAD_PF * 2 - 14)) + ((offset - 8) << 4) + 32) >> 6;
    sil_wrw_mem(NRF5340_OSCILLATORS_XOSC32MCAPS,
                NRF5340_OSCILLATORS_XOSC32MCAPS_CAPVALUE(cap)
              | NRF5340_OSCILLATORS_XOSC32MCAPS_ENABLE);
    sil_wrw_mem(NRF5340_CLOCK_HFCLKSRC, NRF5340_CLOCK_HFCLKSRC_HFXO);
    sil_wrw_mem(NRF5340_CLOCK_HFCLKCTRL, NRF5340_CLOCK_HFCLKCTRL_DIV_1);
    sil_wrw_mem(NRF5340_CLOCK_TASKS_HFCLKSTART, 1);

    sil_wrw_mem(NRF5340_REGULATORS_VREGMAIN_DCDCEN, 1);
    sil_wrw_mem(NRF5340_REGULATORS_VREGH_DCDCEN, 1);

    sil_wrw_mem(NRF5340_UARTE_PSEL_TXD(UART_BASE),
                NRF5340_UARTE_PSEL_PORT(UART_TX_PORT) | NRF5340_UARTE_PSEL_PIN(UART_TX_PIN));
    sil_wrw_mem(NRF5340_UARTE_PSEL_RXD(UART_BASE),
                NRF5340_UARTE_PSEL_PORT(UART_RX_PORT) | NRF5340_UARTE_PSEL_PIN(UART_RX_PIN));
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
    sil_wrw_mem(NRF5340_SPU_EXTDOMAIN_PERM, 0); /* Set network-core non-secure */
    sil_wrw_mem(NRF5340_SPU_DPPI_PERM, 0); /* Set all DPPI channel non-secure */
    sil_wrw_mem(NRF5340_SPU_GPIOPORT(0), 0); /* Set all pins of port 0 non-secure */
    sil_wrw_mem(NRF5340_SPU_GPIOPORT(1), 0); /* Set all pins of port 1 non-secure */

    /*
     * Most of peripherals can be mapped to any pin.
     * However, some pins have dedicated functionalities, e.g., 32.768kHz crystal.
     * These pins have configurations of whether the CPU or a peripheral controls them.
     * Only secure application can change this configuration.
     *
     * P0.00 and P0.01 drives 32.768kHz crystal, and Zephyr relies on the 32.768kHz clock.
     * Therefore, Zephyr cannot operate unless these pins are configured to pheripheral-controlled.
     *
     * The following code sets P0.00 and P0.01 peripheral-controlled.
     */
    sil_wrw_mem(NRF5340_GPIO0_PIN_CNF(0),
                (sil_rew_mem(NRF5340_GPIO0_PIN_CNF(0)) & ~(NRF5340_GPIO_PIN_CNF_MCUSEL_MASK))
                | NRF5340_GPIO_PIN_CNF_MCUSEL_PERIPHERAL); /* XL1: P0_0 */
    sil_wrw_mem(NRF5340_GPIO0_PIN_CNF(1),
                (sil_rew_mem(NRF5340_GPIO0_PIN_CNF(1)) & ~(NRF5340_GPIO_PIN_CNF_MCUSEL_MASK))
                | NRF5340_GPIO_PIN_CNF_MCUSEL_PERIPHERAL); /* XL2: P0_1 */

    /* Zephyr uses these pins for UART from the network core */
    sil_wrw_mem(NRF5340_GPIO1_PIN_CNF(1),
                (sil_rew_mem(NRF5340_GPIO1_PIN_CNF(1)) & ~(NRF5340_GPIO_PIN_CNF_MCUSEL_MASK))
                | NRF5340_GPIO_PIN_CNF_MCUSEL_NETWORKMCU); /* TX: P1_1 */
    sil_wrw_mem(NRF5340_GPIO1_PIN_CNF(0),
                (sil_rew_mem(NRF5340_GPIO1_PIN_CNF(0)) & ~(NRF5340_GPIO_PIN_CNF_MCUSEL_MASK))
                | NRF5340_GPIO_PIN_CNF_MCUSEL_NETWORKMCU); /* RX: P1_0 */
    sil_wrw_mem(NRF5340_GPIO0_PIN_CNF(11),
                (sil_rew_mem(NRF5340_GPIO0_PIN_CNF(11)) & ~(NRF5340_GPIO_PIN_CNF_MCUSEL_MASK))
                | NRF5340_GPIO_PIN_CNF_MCUSEL_NETWORKMCU); /* RTS: P0_11 */
    sil_wrw_mem(NRF5340_GPIO0_PIN_CNF(10),
                (sil_rew_mem(NRF5340_GPIO0_PIN_CNF(10)) & ~(NRF5340_GPIO_PIN_CNF_MCUSEL_MASK))
                | NRF5340_GPIO_PIN_CNF_MCUSEL_NETWORKMCU); /* CTS: P0_10 */

    /* Set all flash regions used by ASP3 secure */
    for (int i = 0; i < ((uintptr_t)&__flash_end - (uintptr_t)&__flash_start) / (16 * 1024); ++i) {
        sil_wrw_mem(NRF5340_SPU_FLASHREGION_PERM(i), NRF5340_SPU_FLASHREGION_PERM_EXECUTE
                                                   | NRF5340_SPU_FLASHREGION_PERM_WRITE
                                                   | NRF5340_SPU_FLASHREGION_PERM_READ
                                                   | NRF5340_SPU_FLASHREGION_PERM_SECURE);
    }
    /* Set the NSC region */
    sil_wrw_mem(NRF5340_SPU_FLASHNSC_REGION(0), ((uintptr_t)&__nsc_start - (uintptr_t)&__flash_start) / (16 * 1024));
    sil_wrw_mem(NRF5340_SPU_FLASHNSC_SIZE(0), (uintptr_t)&__nsc_size);
    /* Set all flash regions used by the guest OS non-secure */
    for (int i = ((uintptr_t)&__flash_end - (uintptr_t)&__flash_start) / (16 * 1024); i < 64; ++i) {
        sil_wrw_mem(NRF5340_SPU_FLASHREGION_PERM(i), NRF5340_SPU_FLASHREGION_PERM_EXECUTE
                                                   | NRF5340_SPU_FLASHREGION_PERM_WRITE
                                                   | NRF5340_SPU_FLASHREGION_PERM_READ);
    }

    /* Set all RAM regions used by ASP3 secure */
    for (int i = 0; i < ((uintptr_t)&__ram_end - (uintptr_t)&__ram_start) / (8 * 1024); ++i) {
        sil_wrw_mem(NRF5340_SPU_RAMREGION_PERM(i), NRF5340_SPU_RAMREGION_PERM_EXECUTE
                                                 | NRF5340_SPU_RAMREGION_PERM_WRITE
                                                 | NRF5340_SPU_RAMREGION_PERM_READ
                                                 | NRF5340_SPU_RAMREGION_PERM_SECURE);
    }
    /* Set all RAM regions used by the guest OS non-secure */
    for (int i = ((uintptr_t)&__ram_end - (uintptr_t)&__ram_start) / (8 * 1024); i < 64; ++i) {
        sil_wrw_mem(NRF5340_SPU_RAMREGION_PERM(i), NRF5340_SPU_RAMREGION_PERM_EXECUTE
                                                 | NRF5340_SPU_RAMREGION_PERM_WRITE
                                                 | NRF5340_SPU_RAMREGION_PERM_READ);
    }

    /* Set all peripherals non-secure */
    for (int i = 0; i < 256; ++i) {
        if (i == 3) continue; /* SPU */
        sil_wrw_mem(NRF5340_SPU_PERIPHID_PERM(i), 0);
    }

    /* Configure secure peripherals */
    sil_wrw_mem(NRF5340_SPU_PERIPHID_PERM(UART_IRQn),
                NRF5340_SPU_PERIPHID_PERM_SECATTR_Secure
              | NRF5340_SPU_PERIPHID_PERM_DMASEC_Secure); /* UART */
    sil_wrw_mem(NRF5340_SPU_PERIPHID_PERM(INTNO_TIMER - 16),
                NRF5340_SPU_PERIPHID_PERM_SECATTR_Secure); /* Timer */

    /* Disable SAU (set all regions non-secure) */
    sil_wrw_mem((uint32_t *)SAU_CTRL, SAU_CTRL_ALLNS);
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
