/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
 * 
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2005-2015 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 * 
 *  上記著作権者は，以下の(1)〜(4)の条件を満たす場合に限り，本ソフトウェ
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
 *  @(#) $Id: core_support.S 1799 2023-04-01 00:50:30Z ertl-komori $
 */

/*
 *  プロセッサ依存モジュール アセンブリ言語部（ARM-M用）
 */

#define TOPPERS_MACRO_ONLY
#define UINT_C(val)		(val)		/* uint_t型の定数を作るマクロ */
#define ULONG_C(val)	(val)		/* ulong_t型の定数を作るマクロ */
#define CAST(type, val)	(val)		/* 型キャストを行うマクロ */

#include "kernel_impl.h"
#include "arm_m.h"
#include "offset.h"
#include "target_asm.inc"

/*
 *  CPU例外エントリ
 */
	AALIGN(2)
	ATEXT
	ATHUMB(core_exc_entry)
	AGLOBAL(core_exc_entry)
ALABEL(core_exc_entry)
	/*
	 *  例外/割込みが発生すると，発生時にアクティブなスタックにスクラ
	 *  ッチレジスタ等が保存される．
	 *  この内容に加えて，basepri の値と EXC_RETURN の情報を加えて保存する．
	 *  basepri は exc_sense_context() および exc_get_iipm() で利用する．
	 *
	 *   ---------------------
	 *  |  primask | basepri  |
	 *   ---------------------
	 *  |      EXC_RETURN     |
	 *   ---------------------
	 *  |          R0         |
	 *   ---------------------
	 *  |          R1         |
	 *   ---------------------
	 *  |          R2         |
	 *   ---------------------
	 *  |          R3         |
	 *   ---------------------
	 *  |         R12         |
	 *   ---------------------
	 *  |          LR         |
	 *   ---------------------
	 *  |          PC         |
	 *   ---------------------
	 *  |         xPSR        |
	 *   ---------------------
	 */
	mrs   r1, basepri
	mrs   r2, primask
	orrs  r1, r1, r2, lsl #8
	tst   lr, #EXC_RETURN_PSP
	beq   core_exc_entry_1
	/* タスクコンテキストで発生した CPU 例外の場合 */
	mrs   r0, psp
	stmfd r0!, {r1, lr} /* 例外フレームの上に basepri と EXC_RETURN を積む */
	push  {r1, lr} /* リターン処理のため MSP 上にも積む */
	b     core_exc_entry_2
ALABEL(core_exc_entry_1)
	/* 非タスクコンテキストで発生した CPU 例外の場合 */
	push  {r1, lr}
	mov   r0, sp
ALABEL(core_exc_entry_2)
	/* CPU ロック状態を保存する */
	ldr   r2, =lock_flag
	ldr   r3, [r2]
	push  {r2, r3}
#ifdef TOPPERS_SUPPORT_OVRHDR
	push  {r0, r1}
	bl    ovrtimer_stop
	pop   {r0, r1}
#endif /* TOPPERS_SUPPORT_OVRHDR */
#ifdef LOG_EXC_ENTER
	push  {r0, r1}
	mrs   r0, ipsr /* 例外番号をパラメータに */
	bl    log_exc_enter /* log_exc_enterを呼び出す */
	pop   {r0, r1}
#endif /* LOG_EXC_ENTER */
	/*
	 *  CPU例外ハンドラの呼び出し
	 */
	mrs   r3, ipsr
	ldr   r1, =_kernel_exc_tbl
	ldr   r2, [r1, r3, lsl #2]
	/* 引数 p_excinf は r0 に設定されている */
	blx   r2
#ifdef LOG_EXC_ENTER
	mrs   r0, ipsr /* 例外番号をパラメータに */
	bl    log_exc_leave /* log_exc_leaveを呼び出す */
#endif /* LOG_EXC_ENTER */
#ifdef TOPPERS_SUPPORT_OVRHDR
	bl    ovrtimer_start
#endif /* TOPPERS_SUPPORT_OVRHDR */
	/* CPU ロック状態と割り込み優先度を元に戻す */
	pop   {r0-r3}
	str   r1, [r0]
	msr   basepri, r2
	bx    r3 /* リターン */

/*
 * 遅延ディスパッチ
 */
	ATHUMB(pendsv_handler)
	AGLOBAL(pendsv_handler)
ALABEL(pendsv_handler)
	/* カーネル管理の割り込みを禁止 */
	movs   r0, #IIPM_LOCK
	msr    basepri, r0
	ldr    r1, =p_runtsk
	ldr    r0, [r1]
	/* p_runtsk == NULL <=> アイドル処理から -> コンテキスト保存なし */
	cbz    r0, pendsv_handler_1
	/* コンテキストを保存 */
	mrs    r2, psp
#ifdef TOPPERS_FPU_CONTEXT
	tst    lr, #EXC_RETURN_FP
	bne    pendsv_handler_0
#ifdef TOPPERS_SAFEG_M
	/*
	 *  SafeG-M: 実行中タスクが BTASK の場合は FPU コンテキストを退避しない．
	 *  BTASK は Non-secure 世界を表すタスクであり，Secure 側バンクの
	 *  s16-s31 は意味を持たない．NS→Secure gate の veneer が NS 復帰前に
	 *  FP スクラッチをゼロ化する標準シーケンスで CONTROL_S.FPCA=1 が
	 *  残るため，これを退避すると BTASK の Secure スタックを超えて
	 *  PSPLIM_S 違反(HardFault)を起こす．退避をスキップし，復帰側でも
	 *  対称にスキップする．
	 */
	ldr    r3, =_kernel_tcb_table
	cmp    r0, r3
	beq    pendsv_handler_0
#endif /* TOPPERS_SAFEG_M */
	vstmdb r2!, {s16-s31}
	/* vstmdb により，Lazy Stacking の際もスクラッチが保存される． */
ALABEL(pendsv_handler_0)
#endif /* TOPPERS_FPU_CONTEXT */
	stmfd  r2!, {r4-r11}
	str    r2, [r0, #TCB_sp]
	str    lr, [r0, #TCB_pc]
ALABEL(pendsv_handler_1)
	/*
	 * この時点でのレジスタ
	 * r0 : p_runtsk
	 * r1 : &p_runtsk
	 */
#ifdef LOG_DSP_ENTER
	push   {r0, r1}
	bl     log_dsp_enter
	pop    {r0, r1}
#endif /* LOG_DSP_ENTER */
	ldr    r0, =p_schedtsk
	ldr    r0, [r0]
	str    r0, [r1] /* p_runtsk = p_schedtsk */
	cbz    r0, return_to_idle /* p_runtsk == NULL なら idle_loop へ */
#ifdef LOG_DSP_LEAVE
	push   {r0, r1}
	bl     log_dsp_leave
	pop    {r0, r1}
#endif /* LOG_DSP_LEAVE */
#if __TARGET_ARCH_THUMB >= 5
	ldr    r1, [r0, #TCB_stk_top]
	msr    psplim, r1
#endif /* __TARGET_ARCH_THUMB >= 5 */
	/* 戻り先が EXC_RETURN でないなら Thread モードへ */
	ldr    r1, [r0, #TCB_pc]
	ands   r2, r1, #EXC_RETURN_PREFIX
	cmp    r2, #EXC_RETURN_PREFIX
	bne    return_to_thread
	ldr    r2, [r0, #TCB_sp]
	ldmfd  r2!, {r4-r11}
#ifdef TOPPERS_FPU_CONTEXT
	tst    r1, #EXC_RETURN_FP
	bne    pendsv_handler_2
#ifdef TOPPERS_SAFEG_M
	/* SafeG-M: BTASK は FPU コンテキストを退避していないため復帰もスキップ */
	ldr    r3, =_kernel_tcb_table
	cmp    r0, r3
	beq    pendsv_handler_2
#endif /* TOPPERS_SAFEG_M */
	vldmia r2!, {s16-s31}
ALABEL(pendsv_handler_2)
#endif /* TOPPERS_FPU_CONTEXT */
	msr    psp, r2
	/*
	 * 割り込み優先度マスク全解除状態へ
	 * この区間でカーネル管理の割り込みが発生しないよう faultmask をセットする．
	 * 例外リターンにより，faultmask は自動的に解除される．
	 */
#ifdef TOPPERS_SAFEG_M
	/* p_runtsk == btask なら Non-secure へ */
	ldr    r2, =_kernel_tcb_table
	cmp    r0, r2
	ite    eq
	moveq  r0, #0 /* Non-secure の割り込みを許可 */
	movne  r0, #IIPM_ENAALL
#else
	movs   r0, #IIPM_ENAALL
#endif /* TOPPERS_SAFEG_M */
	cpsid  f
	msr    basepri, r0
	bx     r1
ALABEL(return_to_idle)
	mrs    r0, psp
	ldr    r1, =idle_loop
	movs   r2, #EPSR_T
	stmfd  r0!, {r1, r2}
	subs   r0, #(4 * 6)
	msr    psp, r0
	/*
	 * GNU AS はバージョンにより ldr pc, =#EXC_RETURN を
	 * mvn pc, #2 のように変換するが，mvn 命令により pc に
	 * EXC_RETURN を代入して例外復帰することはできない．
	 * そのため以下のように ldr 命令を直接記述している．
	 */
	ldr    pc, exc_return_const
ALABEL(return_to_thread)
	/*
	 * この時点でのレジスタ
	 * r0 : p_runtsk
	 * r1 : p_runtsk->tskctxb.pc
	 */
#ifdef TOPPERS_FPU_CONTEXT
	ldr    r2, [r0, #TCB_fpu_flag]
	cbz    r2, return_to_thread_no_fp
	ldr    r2, =return_to_thread_fp
	movs   r3, #EPSR_T
	ldr    r0, [r0, #TCB_sp]
	ldmfd  r0!, {r4-r11}
	stmfd  r0!, {r1-r3}
	subs   r0, #(4 * 5)
	b      return_to_thread_0
ALABEL(return_to_thread_no_fp)
#endif /* TOPPERS_FPU_CONTEXT */
	ldr    r0, [r0, #TCB_sp]
	ldmfd  r0!, {r4-r11}
	movs   r2, #EPSR_T
	stmfd  r0!, {r1, r2}
	subs   r0, #(4 * 6)
#ifdef TOPPERS_FPU_CONTEXT
ALABEL(return_to_thread_0)
#endif /* TOPPERS_FPU_CONTEXT */
	msr    psp, r0
	/* CPU ロック状態へ */
	ldr    r0, =lock_flag
	movs   r1, #1
	str    r1, [r0]
	dsb /* lock_cpu_dsp() のコメント参照 */
	ldr    pc, exc_return_const /* return_to_idle のコメント参照 */
	/*
	 * この時点でのレジスタ
	 * lr  : p_runtsk->tskctxb.pc
	 */
#ifdef TOPPERS_FPU_CONTEXT
ALABEL(return_to_thread_fp)
	/* 例外リターンにより CONTROL.FPCA がクリアされるが，ここでまたセットされる． */
	vpop   {s16-s31}
	bx     lr
#endif /* TOPPERS_FPU_CONTEXT */

ALABEL(exc_return_const)
	.word  EXC_RETURN

/*
 * Thread モードから Handler モードへの復帰
 * dispatch -> do_dispatch から呼ばれる
 * この時点でのレジスタ
 * r4 : p_runtsk
 * r5 : p_runtsk->tskctxb.pc
 * r6 : basepri に設定する値 (SafeG-M)
 */
	ATHUMB(svc_handler)
	AGLOBAL(svc_handler)
ALABEL(svc_handler)
#ifdef LOG_DSP_LEAVE
	movs   r0, r4
	bl     log_dsp_leave
#endif /* LOG_DSP_LEAVE */
	/* CPU ロック解除状態へ */
	ldr    r0, =lock_flag
	movs   r1, #0
	str    r1, [r0]
#ifdef TOPPERS_SAFEG_M
	msr    basepri, r6
#else
	movs   r0, #IIPM_ENAALL
	msr    basepri, r0
#endif /* TOPPERS_SAFEG_M */
#if __TARGET_ARCH_THUMB >= 5
	ldr    r0, [r4, #TCB_stk_top]
	msr    psplim, r0
#endif
	ldr    r0, [r4, #TCB_sp]
	movs   r1, r5
#if defined(TOPPERS_FPU_CONTEXT) && defined(TOPPERS_SAFEG_M)
	/*
	 *  SafeG-M: BTASK の FPU コンテキスト復帰スキップ判定を，
	 *  直後の ldmfd で r4 が破壊される前に行う(r3 == 0 のとき BTASK)．
	 */
	ldr    r3, =_kernel_tcb_table
	subs   r3, r4, r3
#endif
	ldmfd  r0!, {r4-r11}
#ifdef TOPPERS_FPU_CONTEXT
	tst    r1, #EXC_RETURN_FP
	bne    svc_handler_0
#ifdef TOPPERS_SAFEG_M
	cbz    r3, svc_handler_0 /* BTASK は FPU 復帰をスキップ */
#endif /* TOPPERS_SAFEG_M */
	vldmia r0!, {s16-s31}
ALABEL(svc_handler_0)
#endif /* TOPPERS_FPU_CONTEXT */
	msr    psp, r0
	bx     r1

/*
 *  ディスパッチャの動作開始
 */
	ATHUMB(start_dispatch)
	AGLOBAL(start_dispatch)
ALABEL(start_dispatch)
	/*
	 *  このルーチンは，カーネル起動時に，すべての割込みを禁止した状態
	 * （割込みロック状態と同等）で呼び出される．また，割込みモード（非
	 *  タスクコンテキストと同等）で呼び出されることを想定している．
	 *
	 *  core_initializeで，lock_flagをtrueに，saved_iipmをIIPM_ENAALLに
	 *  初期化しているため，カーネル管理外の割込みを許可することで，
	 *  CPUロック状態・（モデル上の）割込み優先度マスク全解除状態になる．
	 *  また，task_initializeでdisdspをfalseに初期化しているため，ディ
	 *  スパッチ許可状態になっている．
	 */
#if __TARGET_ARCH_THUMB >= 5
	ldr   r0, =istk
	ldr   r0, [r0]
	msr   msplim, r0
#endif /* __TARGET_ARCH_THUMB >= 5 */
	ldr   r0, =istkpt             /* MSPを初期化   */
	ldr   r1, [r0]                /* start_dispatch呼び出し時に呼び出し用に */
	msr   msp, r1                 /* 使用しているため初期化する             */
	/* 
	 * スタックをIDが1のタスクのスタック領域に切り換える
	 */
	movs  r0, #CONTROL_INIT       /* PSPを有効に  */
	msr   control, r0
	isb                           /* control の操作後に必要 */
	ldr   r1, =tinib_table
	ldr   sp, [r1, #TINIB_stk_bottom]

	/* dispatcher_0 の引数を設定 */
	ldr   r1, =p_schedtsk
	ldr   r1, [r1]
	ldr   r2, =p_runtsk

	cpsie f                       /* カーネル管理外の割込みを許可 */
	b     dispatcher_0

/*
 *  現在のコンテキストを捨ててディスパッチ
 */
	ATHUMB(exit_and_dispatch)
	AGLOBAL(exit_and_dispatch)
ALABEL(exit_and_dispatch)
#ifdef TOPPERS_FPU_CONTEXT
	/* FPU コンテキストを捨てる */
	movs  r3, #CONTROL_INIT
	msr   control, r3 /* FPCA をクリア */
	isb
#endif /* TOPPERS_FPU_CONTEXT */
	/* dispatcher_0 の引数を設定 */
	ldr   r1, =p_schedtsk
	ldr   r1, [r1]
	ldr   r2, =p_runtsk
	b     dispatcher_0

/*
 * ディスパッチャ本体
 * 引数
 * r0: p_runtsk
 * r1: p_schedtsk
 * r2: &p_runtsk
 */
	ATHUMB(do_dispatch)
	AGLOBAL(do_dispatch)
ALABEL(do_dispatch)
#ifdef TOPPERS_FPU_CONTEXT
#ifdef TOPPERS_SAFEG_M
	/*
	 *  SafeG-M: BTASK は FPU コンテキストを持たない扱いとし退避しない．
	 *  NS→Secure gate の veneer が NS 復帰前に FP スクラッチをゼロ化して
	 *  CONTROL_S.FPCA=1 を残すため，これを vpush すると BTASK の
	 *  Secure スタックを超えて PSPLIM_S 違反(HardFault)となる．
	 */
	ldr   r3, =_kernel_tcb_table
	cmp   r0, r3
	beq   dispatch_btask_nofp
#endif /* TOPPERS_SAFEG_M */
	mrs   r3, control
	ands  r3, #CONTROL_FPCA
	beq   dispatch_1 /* FPU の使用がなければ保存をスキップ */
	vpush {s16-s31}
	movs  r3, #CONTROL_INIT
	msr   control, r3 /* FPCA をクリア */
	isb
#ifdef TOPPERS_SAFEG_M
	b     dispatch_1
ALABEL(dispatch_btask_nofp)
	movs  r3, #CONTROL_INIT
	msr   control, r3 /* veneer scratch 由来の FPCA をクリア */
	isb
	movs  r3, #0      /* fpu_flag = 0 (FP コンテキストなし) */
#endif /* TOPPERS_SAFEG_M */
ALABEL(dispatch_1)
	/* fpu_flag はゼロもしくは CONTROL_INIT と同じ値（非ゼロ） */
	str   r3, [r0, #TCB_fpu_flag]
#endif /* TOPPERS_FPU_CONTEXT */
	push  {r4-r11}
	str   lr, [r0, #TCB_pc]
	str   sp, [r0, #TCB_sp]
ALABEL(dispatcher_0)
#ifdef LOG_DSP_ENTER
	push  {r1, r2}
	bl    log_dsp_enter
	pop   {r1, r2}
#endif /* LOG_DSP_ENTER */
	str   r1, [r2] /* p_runtsk = p_schedtsk */
	cbz   r1, idle_loop /* p_runtsk == NULL ならアイドル処理へ */
	ldr   r2, [r1, #TCB_pc]
	/* 実行再開番地が EXC_RETURN なら Handler モードへ */
	ands  r0, r2, #EXC_RETURN_PREFIX
	cmp   r0, #EXC_RETURN_PREFIX
	bne   dispatcher_1
	/* r1, r2 は例外ハンドラの先頭で未定義となるため退避 */
	movs  r4, r1
	movs  r5, r2
#ifdef TOPPERS_SAFEG_M
	ldr   r3, =_kernel_tcb_table
	cmp   r1, r3
	ite   eq
	moveq r6, #0
	movne r6, #IIPM_ENAALL /* basepri の設定に使用する */
#endif /* TOPPERS_SAFEG_M */
	svc   #0

/*
 * Thread モードでのコンテキスト復帰
 * この時点でのレジスタ
 * r1 : p_runtsk
 * r2 : p_runtsk->tskctxb.pc
 */
ALABEL(dispatcher_1)
#ifdef LOG_DSP_LEAVE
	push  {r1, r2}
	movs  r0, r1
	bl    log_dsp_leave
	pop   {r1, r2}
#endif /* LOG_DSP_LEAVE */
#if __TARGET_ARCH_THUMB >= 5
	/*
	 * psplim の設定中にカーネル管理外の割り込み/例外が発生して
	 * 例外フレームが積まれてもスタックオーバーフローとならないようにする．
	 * ただし，本当にスタックオーバーフローした場合も検知できない．
	 * sense_context()の動作を保証する必要はないので，一旦MSPに
	 * 切り替えても良いが処理が多くなる．
	 */
	movs  r0, #0
	msr   psplim, r0
	ldr   sp, [r1, #TCB_sp]
	ldr   r0, [r1, #TCB_stk_top]
	msr   psplim, r0
ALABEL(dispatcher_1_after_sp)
#else
	ldr   sp, [r1, #TCB_sp]
#endif /* __TARGET_ARCH_THUMB >= 5 */
	pop   {r4-r11}
#ifdef TOPPERS_FPU_CONTEXT
	ldr   r3, [r1, #TCB_fpu_flag]
	cbz   r3, dispatcher_2
	vpop  {s16-s31} /* CONTROL.FPCA もセットされる */
ALABEL(dispatcher_2)
#endif /* TOPPERS_FPU_CONTEXT */
	bx    r2

ALABEL(idle_loop)
#ifdef TOPPERS_CUSTOM_IDLE
	toppers_asm_custom_idle
#else
	/* CPU ロック解除状態へ */
	ldr  r0, =lock_flag
	movs r1, #0
	str  r1, [r0]
	dsb  /* unlock_cpu_dsp() のコメント参照 */
	movs r0, #IIPM_ENAALL
	msr  basepri, r0
ALABEL(_idle_loop_1)
	/*
	 *  basepriを全割込み許可に設定してからwfiが発行されるまでに
	 *  割込みが入った場合，そこでディスパッチが必要な状態になれば
	 *  割込みの出口処理で遅延ディスパッチし，ここには戻ってこない．
	 *  遅延ディスパッチしなかった場合はここに戻ってきて次の割込みを
	 *  wfiで待つため，想定どおりの動作となる
	 */
	dsb /* スリープ前に全てのメモリアクセスが完了していることを保証する */
	wfi
	b     _idle_loop_1
	nop
#endif /* TOPPERS_CUSTOM_IDLE */

/*
 * タスクの起動
 */
	ATHUMB(start_r)
	AGLOBAL(start_r)
ALABEL(start_r)
#ifdef TOPPERS_SUPPORT_OVRHDR
	bl  ovrtimer_start
#endif /* TOPPERS_SUPPORT_OVRHDR */
	/* CPU ロック解除状態へ */
	ldr  r0, =lock_flag
	movs r1, #0
	str  r1, [r0]
	dsb  /* unlock_cpu_dsp() のコメント参照 */
	movs r0, #IIPM_ENAALL
	msr  basepri, r0
	ldr  r0, =p_runtsk
	ldr  r1, [r0]
	ldr  r2, [r1, #TCB_p_tinib]
	ldr  r0, [r2, #TINIB_exinf]
	ldr  lr, =ext_tsk
	ldr  pc, [r2, #TINIB_task]

/*
 *  カーネルの終了処理の呼出し
 *
 *  スタックを非タスクコンテキスト用に切り替え．
 *  
 */
	ATHUMB(call_exit_kernel)
	AGLOBAL(call_exit_kernel)
ALABEL(call_exit_kernel)
	movs  r1, #CONTROL_MSP
	msr   control, r1  /* MSPを有効に  */
	isb                /* control の操作後に必要 */
	b     exit_kernel  /* カーネルの終了処理を呼ぶ */

/*
 *  微少時間待ち
 */
	AALIGN(2)
	ATEXT
	ATHUMB(sil_dly_nse)
	AGLOBAL(sil_dly_nse)
ALABEL(sil_dly_nse)
	sub   r0, #SIL_DLY_TIM1
	cmp   r0, #0
	bgt   sil_dly_nse1
	bx    lr
ALABEL(sil_dly_nse1)
	sub   r0, #SIL_DLY_TIM2
	cmp   r0, #0
	bgt   sil_dly_nse1
	bx    lr

#ifdef TOPPERS_SAFEG_M

	ATHUMB(usagefault_handler)
	AGLOBAL(usagefault_handler)
ALABEL(usagefault_handler)
	/*
	 * Usagefault 例外ハンドラ
	 * 通常の Usagefault であるか，deactivate_first_entry で意図的に発生させた
	 * Usagefault かを判定する．
	 * 意図的な Usagefault であるかは次の手順で判定する．
	 * 1. 例外が Secure の Thread モードで発生し，
	 *    コンテキスト保存に PSP が使用されたことを確かめる．
	 * 2. 戻り先が deactivate_first_entry の udf 命令であることを確かめる．
	 * これらに当てはまらない場合は core_exc_entry へ分岐する．
	 *
	 * このハンドラは最高優先度で実行されるので，なるべく早く終了させる．
	 */
	and   r0, lr, #(EXC_RETURN_S | EXC_RETURN_THREAD | EXC_RETURN_PSP)
	cmp   r0, #(EXC_RETURN_S | EXC_RETURN_THREAD | EXC_RETURN_PSP)
	bne   core_exc_entry
	mrs   r0, psp
	ldr   r1, [r0, #(4 * 6)]
	ldr   r2, =deactivate_udef_entry
	cmp   r1, r2
	bne   core_exc_entry
	/* この時点で意図的な Usagefault であることが確定 */
	/* udf の次命令にリターンするよう設定 */
	add   r1, #2
	str   r1, [r0, #(4 * 6)]
	/* Usagefault のステータスをクリア */
	ldr   r0, =#SCB_UFSR
	mov   r1, #1
	strh  r1, [r0]
	/* 1回目はダミーのレジスタ情報をスタックに積む */
	/* NVIC_IABR は読み取り専用なので安全 */
	push  {r4, r5}
	ATHUMB(deactivate_nest_entry)
ALABEL(deactivate_nest_entry)
	/* MSP にダミーコンテキストを準備 */
	stmfd sp!, {r7, r8} /* RETPSR と戻り先番地を設定 */
	sub   sp, #(4 * 6)
	ldr   pc, =#EXC_RETURN_NESTED /* 例外リターンで deactivate_exc_point へ */

/*
 * Non-secure でアクティブなままになっている割り込みを完了させる．
 * アクティブな割り込みがなければ，このルーチンは終始 Thread モードで動作する．
 * 最初にアクティブな割り込みを発見すると Usagefault を発生させ，Handler モードへ移行する．
 * その後 Usagefault のハンドラから対象の割り込みのハンドラへ移行し実行を続ける．
 * 2回目にアクティブな割り込みを発見した場合はそのまま次の割り込みハンドラへ移行する．
 * これを繰り返し最後の割り込みハンドラから Thread モードへリターンし，終了する．
 * Handler モードである間の実行優先度は Usagefault のハンドラである間を除いて 0x80 である．
 * すなわち，ほとんど全ての時間でこのルーチンは割り込みによりプリエンプト可能である．
 *
 * Thread モードと 複数の Handler モードでレジスタの値を共有するため，
 * 次のように非スクラッチレジスタを利用する．
 * r4  : NVIC non-secure active bit register 0
 * r5  : loop counter
 * r6  : アクティブな割り込みに対する分岐先
 * r7  : 例外リターンの戻り先 (deactivate_exc_point)
 * r8  : RETPSR
 * r9  : 処理済みの割り込みを分別するマスク
 * r10 : NVIC target nonsecure register のバックアップ
 * r11 : リターン先アドレス
 *
 * なお，r0 は launch_ns の引数として使用するため変更できない．
 */
	ATHUMB(deactivate_nonsecure_interrupts)
	AGLOBAL(deactivate_nonsecure_interrupts)
ALABEL(deactivate_nonsecure_interrupts)
	/* Non-secure の例外をデアクティベート */
	ldr  r1, =#SCB_NS_SHCSR /* SCB_NS.SHCSR */
	mov  r2, #0
	str  r2, [r1]
	ldr  r4, =#NVIC_NS_IABR0 /* r4 は常に NVIC_NS.IABR0 */
	mov  r5, #0 /* r5 は 0 から 15 の間で変化させる */
	/* 初めて例外を発見した際は deactivate_first_entry に分岐する */
	ldr  r6, =deactivate_first_entry
	ldr  r7, =deactivate_exc_point /* r7 は常に deactivate_exc_point */
	/* アクティブな例外がなかった場合はそのまま launch_ns に分岐 */
	ldr  r11, =launch_ns
ALABEL(deactivate_loop_start)
	cmp  r5, #16
	blt  deactivate_loop_body
	/* NVIC_NS.IABR を全て調べ終わるとここに来る */
	/* 戻り先は launch_ns または 例外リターンにより deactivate_udef_entry の udf 命令後 */
	bx   r11
	/* for (r5 = 0; r5 < 16; ++r5) { */
ALABEL(deactivate_loop_body)
	mvn  r9, #0 /* 新しい NVIC_NS.IABRn に対してマスクを全て 1 に初期化 */
ALABEL(deactivate_loop2_start)
	/*
	 * 割り込みのアクティブ状態を取得してマスクする．
	 * いま，IRQ A, B が同じ NVIC_NS.IABR 内でアクティブであったとする．
	 * 最初ここに到達した際は A, B 両方がアクティブであり，A の割り込みハンドラへ分岐する．
	 * 次にここに到達した際は，A の割り込みハンドラとして実行されることになる．
	 * この時まだ A はアクティブであるから，処理の対象から A を外す必要がある．
	 * そこで 2 回目は A をマスクすれば，B の割り込みハンドラへ分岐することができる．
	 */
	ldr  r1, [r4, r5, lsl #2]
	and  r1, r9
	cbz  r1, deactivate_loop_end /* アクティブな例外がなければ次のレジスタへ */
	/* 最上位の 1 であるビットを処理対象にする．*/
	mov  r2, #31
	clz  r1, r1
	sub  r1, r2, r1 /* r1 に最上位ビットが何ビット目であるかが格納される */
	mov  r10, #1
	lsl  r10, r1 /* r10 が最上位ビットに等しくなる (1 << r1) */
	bic  r9, r10 /* マスクを更新し，この割り込みを次回の処理対象から外す */
	mov  r3, #32
	mul  r3, r5
	add  r1, r3 /* r1 が割り込み番号になる */
	ldr  r3, =#NVIC_PRI0 /* NVIC priority register */
	mov  r12, #IIPM_ENAALL
	/* 
	 * 優先度を Thread モードと同じにする．
	 * Secure の割り込みでプリエンプト可能であり，ペンディングされても発行されない．
	 */
	strb r12, [r3, r1]
	/* 一旦割り込みの対象を Secure にする */
	ldr  r3, =#NVIC_ITNS0
	ldr  r2, [r3, r5, lsl #2]
	bic  r12, r2, r10
	str  r12, [r3, r5, lsl #2]
	add  r8, r1, #16 /* r8 が例外番号になる */
	movt r8, #(EPSR_T >> 16)
	/*
	 * 初めてここに到達した際は deactivate_first_entry に分岐し，Handler モードへ移行する．
	 * 2 回目以降はすでに Handler モードであるから，直接 deactivate_nest_entry に分岐する．
	 */
	bx   r6
	ATHUMB(deactivate_exc_point)
ALABEL(deactivate_exc_point)
	/* ここへは 1 つ前の命令から例外リターンで分岐する */
	/* 例外リターンにより inactive となった 1 つ前の割り込みを Non-secure に戻す */
	pop  {r1, r2}
	ldr  r3, [r1]
	orr  r2, r3
	str  r2, [r1]
	/* 次回の例外リターン後に Non-secure に戻せるよう，今回の割り込みをスタックに保存 */
	ldr  r1, =#NVIC_ITNS0
	lsl  r2, r5, #2
	add  r1, r2
	push {r1, r10}
	/* これらスタック操作は全て MSP に対して行われることに気を付ける */
	b    deactivate_loop2_start /* 次の割り込みを調べる */
	/* } */
ALABEL(deactivate_loop_end)
	add  r5, #1
	b    deactivate_loop_start /* 次の NVIC_NS.IABRn へ */

	ATHUMB(deactivate_first_entry)
ALABEL(deactivate_first_entry)
	/* ここへは最初にアクティブな割り込みを発見した際に分岐する */
	/* 次回以降は直接 deactivate_nest_entry へ分岐させる */
	ldr  r6, =deactivate_nest_entry
	/* 終了時は直接 launch_ns ではなく deactivate_exit へ分岐させる */
	ldr  r11, =deactivate_exit
ALABEL(deactivate_udef_entry)
	udf  #0 /* Usagefault を発生させる */
	/* ここへは deactivate_exit から例外リターンで分岐する */
	/* 最後の割り込みを Non-secure に戻す */
	ldr  r1, [r4]
	orr  r5, r1
	str  r5, [r4]
	b    launch_ns /* launch_ns へ分岐 */

	ATHUMB(deactivate_exit)
ALABEL(deactivate_exit)
	pop  {r4, r5} /* 最後の割り込みを Non-secure に戻すためスタックから復帰 */
	ldr  pc, =#EXC_RETURN /* 例外リターンで deactivate_udef_entry の udf 命令後へ */

#endif /* TOPPERS_SAFEG_M */
