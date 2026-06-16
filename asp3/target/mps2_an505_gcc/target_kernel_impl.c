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
 * ターゲット依存モジュール（ARM MPS2-AN505 / QEMU 用）
 */
#include "kernel_impl.h"
#include "target_syssvc.h"
#include <sil.h>
#include "mps2_an505_dk.h"

/*
 *  System Control Block（FPU 有効化に使用）
 */
#define CPACR    ((volatile uint32_t *) 0xE000ED88U)
#define CPACR_CP10_CP11_FULL  (0xFU << 20)

/*
 * 起動時のハードウェア初期化処理
 *
 *  QEMU の mps2-an505 では，FPU（CP10/CP11）を CPACR で有効化し，DSB/ISB
 *  でバリアを張ってから最初の FP 命令を実行する必要がある（ISB が無いと
 *  FP 命令が NOCP UsageFault になる）．クロック等は QEMU 既定で動作するた
 *  め追加設定は不要．
 */
void hardware_init_hook(void)
{
    *CPACR |= CPACR_CP10_CP11_FULL;
    __asm volatile ("dsb 0xf" ::: "memory");
    __asm volatile ("isb 0xf" ::: "memory");
}

#ifndef TOPPERS_OMIT_TECS
/*
 *  システムログの低レベル出力のための初期化
 */
extern void tPutLogSIOPort_initialize(void);
#endif

/*
 * ターゲット依存部 初期化処理
 */
#ifdef TOPPERS_SAFEG_M
/*
 *  MPC (Memory Protection Controller) レジスタ
 *
 *  QEMU AN505(IoTKit) の MPC は既定で全 SSRAM ブロックを Secure 扱いにする．
 *  そのため NS 側が使用する SSRAM 領域（NS コード/NS RAM）を MPC で Non-secure
 *  に設定しないと，NS 実行も launch_ns による NS ベクタ読出し（Secure からの
 *  NS エイリアスアクセス）もバスフォルトする．
 *
 *    MPC0 (0x58007000) : ssram-0 (0x00000000/0x10000000, 4MB)
 *                        前半2MB=Secure カーネル, 後半2MB=NS コード
 *    MPC2 (0x58009000) : ssram-2 (0x28200000/0x38200000, 2MB) = NS RAM
 *  （Secure RAM は ssram-1=MPC1=0x58008000 にあり既定 Secure のまま）
 */
#define MPC0_BASE    0x58007000U
#define MPC2_BASE    0x58009000U
#define MPC_BLK_MAX  0x10U
#define MPC_BLK_CFG  0x14U
#define MPC_BLK_IDX  0x18U
#define MPC_BLK_LUT  0x1CU

/*
 *  指定 MPC の，当該 SSRAM 内オフセット [off_start, off_end) を含む LUT ワード
 *  を Non-secure に設定する（LUT ビット=1 で Non-secure）．
 *
 *  1 LUT ワード = 32 ブロック分．本ターゲットでは NS 領域が LUT ワード境界
 *  （32×blocksize）に整列しているため，ワード単位で 0xFFFFFFFF を書く．
 *  範囲外ワードへの書込みを避けるため BLK_MAX（=ワード数-1）でクランプする．
 */
static void
an505_mpc_set_ns(uint32_t base, uint32_t off_start, uint32_t off_end)
{
    uint32_t blksize  = 1U << (sil_rew_mem((uint32_t *)(base + MPC_BLK_CFG)) + 5);
    uint32_t per_word = blksize * 32U;                  /* 1 LUT ワードが覆うバイト数 */
    uint32_t nwords   = sil_rew_mem((uint32_t *)(base + MPC_BLK_MAX)) + 1U;
    uint32_t w0 = off_start / per_word;
    uint32_t w1 = (off_end + per_word - 1U) / per_word; /* 切り上げ, exclusive */
    uint32_t w;

    if (w1 > nwords) {
        w1 = nwords;
    }
    for (w = w0; w < w1; w++) {
        sil_wrw_mem((uint32_t *)(base + MPC_BLK_IDX), w);
        sil_wrw_mem((uint32_t *)(base + MPC_BLK_LUT), 0xFFFFFFFFU);
    }
}
#endif /* TOPPERS_SAFEG_M */

void target_initialize(void)
{
    /*
     *  コア依存部の初期化
     */
    core_initialize();

    /*
     *  SIO を初期化
     */
#ifndef TOPPERS_OMIT_TECS
    tPutLogSIOPort_initialize();
#endif /* TOPPERS_OMIT_TECS */

#ifdef TOPPERS_SAFEG_M
    /*
     *  SAU 設定（AN505 アドレス空間, mimxrt685 の構成を踏襲）
     *  Secure: code 0x10000000 / RAM 0x38000000，NSC: 0x101FFE00，
     *  NS: code エイリアス 0x00200000 / RAM エイリアス 0x28200000．
     *  ITNS（割込みの NS 割当て）は core_initialize / config_int が処理する．
     */
    /* NSC region（Secure Gateway veneer） */
    sil_wrw_mem((uint32_t *) SAU_RNR, 0);
    sil_wrw_mem((uint32_t *) SAU_RBAR, 0x101FFE00);
    sil_wrw_mem((uint32_t *) SAU_RLAR,
                (0x101FFFFF & SAU_RLAR_LADDR_MASK) | SAU_RLAR_NSC | SAU_RLAR_ENABLE);
    /* NS region（code: 0x00200000..0x003FFFFF, 2MB） */
    sil_wrw_mem((uint32_t *) SAU_RNR, 1);
    sil_wrw_mem((uint32_t *) SAU_RBAR, 0x00200000);
    sil_wrw_mem((uint32_t *) SAU_RLAR,
                (0x003FFFFF & SAU_RLAR_LADDR_MASK) | SAU_RLAR_ENABLE);
    /* NS region（RAM: 0x28200000..0x283FFFFF, 2MB） */
    sil_wrw_mem((uint32_t *) SAU_RNR, 2);
    sil_wrw_mem((uint32_t *) SAU_RBAR, 0x28200000);
    sil_wrw_mem((uint32_t *) SAU_RLAR,
                (0x283FFFFF & SAU_RLAR_LADDR_MASK) | SAU_RLAR_ENABLE);
    sil_wrw_mem((uint32_t *) SAU_CTRL, SAU_CTRL_ENABLE);

    /*
     *  MPC 設定：NS が使用する SSRAM 領域を Non-secure 化する．
     *  （これが無いと NS 実行・launch_ns の NS ベクタ読出しがバスフォルトする）
     *  - ssram-0 後半 2MB（offset 0x200000..0x400000）= NS コード 0x00200000〜
     *  - ssram-2 全体（offset 0x000000..0x200000）     = NS RAM 0x28200000〜
     */
    an505_mpc_set_ns(MPC0_BASE, 0x00200000U, 0x00400000U);
    an505_mpc_set_ns(MPC2_BASE, 0x00000000U, 0x00200000U);

    __asm volatile ("isb 0xf" ::: "memory");
    __asm volatile ("dsb 0xf" ::: "memory");
#endif /* TOPPERS_SAFEG_M */
}

/*
 * ターゲット依存部 終了処理
 *
 *  QEMU 上では Angel/ARM セミホスティングの SYS_EXIT を発行して QEMU を
 *  終了させる（-semihosting-config enable=on 前提）．これによりテストを
 *  「1 コマンドで pass/fail」化できる．
 */
void target_exit(void)
{
    core_terminate();
#ifdef TOPPERS_USE_QEMU
    {
        register uint32_t r0 __asm("r0") = 0x18U;       /* SYS_EXIT */
        register uint32_t r1 __asm("r1") = 0x20026U;    /* ADP_Stopped_ApplicationExit */
        __asm volatile ("bkpt 0xab" : : "r"(r0), "r"(r1) : "memory");
    }
#endif /* TOPPERS_USE_QEMU */
    while (1) ;
}

/*
 * エラー発生時の処理
 */
void Error_Handler(void)
{
    while (1) ;
}

/*
 *  デフォルトの software_term_hook（weak 定義）
 */
__attribute__((weak))
void software_term_hook(void)
{
}
