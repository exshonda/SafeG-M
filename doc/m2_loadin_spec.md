# M2 — SafeG-M → asp3_core 載せ替え仕様（設計）

最終更新: 2026-06-17
位置づけ: M0 凍結（`M0_design_freeze.md` 案1=asp3_core へ `#ifdef TOPPERS_SAFEG_M` 取込み）を受けた **M2 の載せ替え設計**。本書はコードを書かない（設計のみ）。実装は M2 実装フェーズで行い、その際に各サイトへ `/* 【SAFEG】#N */` を付し DIVERGENCE_MAP に追記する（§7）。

参照リポジトリ（読み取り解析のみ・本作業でソース改変なし）:
- asp3_core: `/home/honda/TOPPERS/ASP3CORE/asp3_core`（CMake/非TECS/3.7.2）
- SafeG-M: `/home/honda/TOPPERS/SafeG-M/SafeG-M`（asp3_3.7 ベース・TZ 済）

## 0. M1 成果との接続（本書の対象外＝着手済の前提）
M1 が以下を実施済（本書の各サイトはこれに積層する）。file:line は確認実測。
- `asp3_core/arch/arm_m_gcc/common/core_support.S` の **③pendsv_handler / ④svc_handler の `#else` swap**（本書 §1 の swap2 は M1 担当のため本書では「接続点」のみ記す）。
- CMake plumbing（`ENABLE_SAFEG_M`→`-DTOPPERS_SAFEG_M`、TRUSTZONE 自動有効化）。
- `arm_m.h` のガード／EXC_RETURN 純追加: 実測で **L55-56**（`#if defined(TOPPERS_SAFEG_M) && !defined(TOPPERS_ENABLE_TRUSTZONE)` → `#error`）、**L86-90**（`#ifdef TOPPERS_ENABLE_TRUSTZONE` 下に `EXC_RETURN_S 0x40` / `EXC_RETURN_NESTED 0xFFFFFFF1`、コメントに `【SAFEG】`）。
- asp3_core 側は既に C1 完了済: bare `CPACR`/`FPCCR` を持たず `CPACR_BASE`(L223)/`FPCCR_ADDR`(L232) を使用。`EXC_RETURN_PREFIX` も `#ifndef` ガード済（L78、CMSIS 衝突回避）。

M2 が積む残りは: core_support.S の **additive 7 サイト**、arm_m.h の **C2 純追加群**（SAU/SCB_NS/NSACR/ITNS/IABR/FPCCR_NS/TT_RESP ほか）、`core_kernel_impl.{c,h}` / `core_insn.h` / `core_rename.{def,h}`/`unrename.h`、chip(imxrt600)。

## 0.1 サイト番号の対応（M0 §5 と実測の差）
M0 §5 の SafeG 版行番号は概数。本書は **SafeG 版実測行**で番号を再固定する。SafeG `core_support.S` の `#ifdef TOPPERS_SAFEG_M` 実測サイト（10 個の `#ifdef`、うち④svc-FPU は L337+L349 の 2 ブロックで 1 論理サイト＝計 9 論理サイト）:

| # | SafeG実測行 | ルーチン(SafeG) | 種別 | M2/M1 |
|--:|---|---|---|---|
| ① | L162-175 | pendsv_handler FP save | additive | **M2** |
| ② | L217-222 | pendsv_handler FP restore | additive | **M2** |
| ③ | L232-241 | pendsv_handler 末尾 basepri | **#else swap** | M1 |
| ④ | L325-330＋L337-344＋L349-351 | svc_handler basepri / FP restore skip | **#else swap + additive** | M1(swap)／**M2(FP skip)** |
| ⑤ | L429-454 | do_dispatch FP save | additive | **M2** |
| ⑥ | L478-484 | do_dispatch dispatcher_0 basepri(r6) 算出 | additive | **M2** |
| ⑦ | L606-777 | usagefault_handler / deactivate 群 | additive(自己完結) | **M2** |

注: M0 §5 が①②=FP save、⑤⑥⑦=FP restore、⑧=例外、⑨=自己完結、と分類していたのは概略。実測では FP save/restore が pendsv と do_dispatch と svc の 3 ルーチンに散在する（下表で正確化）。M0 §5 の「⑨」が本書の⑦（deactivate 群）に対応。

---

## 1. `core_support.S`（additive サイト）

asp3_core 版 552 行 / SafeG 版 777 行。両者は **③④の `#else` を除けば構造同一**だが、asp3_core 版が **`__ARM_FEATURE_MVE` 経路と `.balign 4`** を FPU パスに追加している点が最大の構造差（下記 ★MVE）。

### 1.1 挿入位置マッピング表（asp3_core 側 file:line ＝挿入アンカー）

| # | 内容(SafeG由来) | asp3_core 挿入位置(file:line / ルーチン) | #ifdef方針 | 難度 | リスク |
|--:|---|---|---|:--:|---|
| ① | BTASK の FP **退避**スキップ（`p_runtsk==_kernel_tcb_table` なら `pendsv_handler_0` へ分岐） | `core_support.S` **L161 と L162 の間**（`bne pendsv_handler_0` の直後、`vstmdb r2!,{s16-s31}` の直前）／`pendsv_handler` | `#ifdef TOPPERS_SAFEG_M` で `ldr r3,=_kernel_tcb_table; cmp r0,r3; beq pendsv_handler_0` を挿入 | 中 | 既存ラベル `pendsv_handler_0`(asp3 L164) を流用可。MVE 非関与（save の MVE 退避は do_dispatch 側のみ。pendsv は元から MVE 退避コードなし＝SafeG と同形）。 |
| ② | BTASK の FP **復帰**スキップ（`beq pendsv_handler_2`） | `core_support.S` **L202 と L203 の間**（`bne pendsv_handler_2` の直後、`vldmia r2!,{s16-s31}` の直前）／`pendsv_handler` | 同上、`beq pendsv_handler_2` へ | 中 | ★MVE: asp3 pendsv の vldmia 直前に **MVE の VPR 復帰は無い**（asp3 L203 は素の vldmia）。SafeG と同形＝衝突なし。 |
| ④-FP | svc の BTASK FP 復帰スキップ。`ldmfd` で r4 破壊前に `r3=r4-_kernel_tcb_table` を算出し、後段 `cbz r3` で `svc_handler_0` へ | (a) `r3` 算出: asp3 **L313 と L314 の間**（`movs r1,r5` の後、`ldmfd r0!,{r4-r11}` の前）。(b) skip 判定: asp3 **L317 と L318 の間**（`bne svc_handler_0` の後、`vldmia r0!,{s16-s31}` の前） | (a) `#if defined(TOPPERS_FPU_CONTEXT)&&defined(TOPPERS_SAFEG_M)`、(b) `#ifdef TOPPERS_SAFEG_M` で `cbz r3,svc_handler_0` | 中 | ★MVE: asp3 svc の vldmia 直前にも **MVE 復帰は無い**（asp3 L318 素）。SafeG と同形。③(swap)は M1 済なので本サイトは additive のみ M2。 |
| ⑤ | do_dispatch の BTASK FP 退避スキップ＋`dispatch_btask_nofp` 分岐（veneer 由来 FPCA を `CONTROL_INIT` でクリア、`fpu_flag=0`）。SafeG L429-454。 | asp3 **L393 と L394 の間**（`do_dispatch` ラベル直後、`#ifdef TOPPERS_FPU_CONTEXT` の **内側先頭**＝asp3 L394 の直後、`mrs r3,control` の前）に分岐挿入。`dispatch_btask_nofp:` 本体は asp3 **L405(isb) と L406(dispatch_1) の間**＝SafeG の配置と同形に挿入。 | `#ifdef TOPPERS_SAFEG_M` 2 ブロック（先頭の `cmp/beq dispatch_btask_nofp` ＋ 末尾の `b dispatch_1` / `dispatch_btask_nofp:` 本体） | **高** | ★MVE 最重要: asp3 の do_dispatch FP save は **L398 vpush の直後 L399-402 で `__ARM_FEATURE_MVE` の `vmrs r12,VPR; push {r12}` を追加**している。SafeG L443 の vpush 直後には無い。BTASK スキップ経路（`dispatch_btask_nofp`）は vpush 自体を通らないため MVE push も通らず **対称性は保たれる**が、`dispatch_1` 合流時の **スタック上 VPR ワードの有無**が通常タスクと BTASK で食い違わないか要確認（BTASK は fpu_flag=0 で復帰側が vpop/VPR pop をスキップするので整合。下記 ⑥/復帰側 §1.3 参照）。 |
| ⑥ | do_dispatch dispatcher_0 で `r6` に basepri 設定値を算出（BTASK なら 0、否なら IIPM_ENAALL）。SafeG L478-484。svc(④M1) が `msr basepri,r6` で使用。 | asp3 **L428 と L429 の間**（`movs r5,r2` の後、`svc #0` の前）／`dispatcher_0` 経路 | `#ifdef TOPPERS_SAFEG_M` で `ldr r3,=_kernel_tcb_table; cmp r1,r3; ite eq; moveq r6,#0; movne r6,#IIPM_ENAALL` | 中 | M1 の④swap（`msr basepri,r6`）と **r6 の生産/消費ペア**。④(M1)と⑥(M2)を**必ず同時に**入れること（片方欠けると basepri 不定）。順序上 §6 で M1 直後に置く。 |
| ⑦ | `usagefault_handler` / `deactivate_nest_entry` / `deactivate_nonsecure_interrupts` / `deactivate_exc_point` / `deactivate_first_entry` / `deactivate_udef_entry` / `deactivate_exit` の自己完結群。SafeG L606-777。 | asp3 **L552（`sil_dly_nse1` の `bx lr`）の直後＝ファイル末尾**に丸ごと追加 | `#ifdef TOPPERS_SAFEG_M ... #endif` で囲った独立ブロック | 低 | 独立追加。依存シンボル: `core_exc_entry`(asp3 L62 有)、`launch_ns`(C 側⑩で追加)、`SCB_UFSR`/`SCB_NS_SHCSR`/`NVIC_NS_IABR0`/`NVIC_ITNS0`/`NVIC_PRI0`(arm_m.h §2 で追加。`NVIC_PRI0` は asp3 L173 既存)、`EXC_RETURN_NESTED`/`EXC_RETURN`(arm_m.h 済/既存)、`EXC_RETURN_S|THREAD|PSP`(済/既存)、`EPSR_T`(既存)、`IIPM_ENAALL`(既存)。**全依存が他サイトで充足されることが前提**。 |

注（M0 §5 の「⑧=例外」について）: SafeG 実測では L478 は `do_dispatch` 内（本書⑥）であり、独立した「例外サイト」は存在しない。M0 §5 の⑧は⑥と⑦の中間記述の重複と判断（未確認だが SafeG `core_support.S` 全 `#ifdef` を grep して例外専用サイトは検出されず）。

### 1.2 修正(A)（BTASK FP 退避 skip）の論理が FPU 経路に正しく乗るか
SafeG の修正(A) は **退避(①⑤)と復帰(②④-FP)を対称にスキップ**する設計。判定キーは一貫して `p_runtsk(または p_schedtsk) == _kernel_tcb_table`（＝ID=1 の BTASK が先頭エントリ）。asp3_core 側でも `_kernel_tcb_table` シンボルは同一（rename 系で共通）、`TCB_fpu_flag` オフセットも `core_offset.py` 由来で同一構造のため、**論理は asp3_core の FPU 経路にそのまま乗る**。唯一の差は ★MVE（§1.1 ⑤）で、BTASK 経路は vpush/vpop を通らず MVE push/pop も通らないため、退避量と復帰量がペアで 0 となり整合する（要 M2 実機/QEMU 非遷移ビルドで `__ARM_FEATURE_MVE` 有効構成での確認。Cortex-M85 等。imxrt685=M33 は MVE 無→無関係）。

### 1.3 FPU save-restore 構造差の所見（★MVE と .balign）
- **★MVE（最重要構造差）**: asp3_core が `do_dispatch`(L399-402 push) と `dispatcher_2`(L465-468 pop) と `return_to_thread`(L243-246) に `__ARM_FEATURE_MVE` 経路を追加。SafeG にはこれが無い。M2 の additive サイトは **MVE push/pop の外側**に置く（退避スキップは vpush の手前で分岐、復帰スキップは vpop の手前で分岐）ため、MVE ワードの積み降ろしと干渉しない。ただし **BTASK のスタックフレームに MVE VPR ワードが存在しないこと**を、復帰側 `dispatcher_2`(asp3 L463 `cbz r3,dispatcher_2`) の fpu_flag=0 経路がスキップで保証する設計に依存。⑤の `dispatch_btask_nofp` で `fpu_flag=0` を必ず書くこと（SafeG L453 と同等）。
- **.balign 4（軽微差）**: asp3 L284 `.balign 4`（exc_return_const 整合）。SafeG に無し。additive サイトは exc_return_const より上流なので無関係。持ち込み不要（asp3 側を尊重）。
- **return_to_thread の FP 経路**: SafeG/asp3 とも BTASK 専用分岐は無い（fpu_flag ベース）ので additive サイト対象外。

---

## 2. `arm_m.h`（C2 純追加 / C1 規則適用）

asp3_core 実測で **SAU_* / SCB_NS_* / SCB_AIRCR* / SCB_SCR* / SCB_UFSR / NSACR* / NVIC_ITNS0 / NVIC_NS_IABR0 / FPCCR_NS / TT_RESP_S は一つも存在しない**（grep 0 件＝全て純追加で衝突なし）。M1 が EXC_RETURN_S/_NESTED とガードのみ追加済。

### 2.1 純追加マッピング表

| 定義群(SafeG arm_m.h 実測) | asp3_core 挿入位置 | #ifdef方針 | C1適用 | 難度 |
|---|---|---|---|:--:|
| `NVIC_ITNS0 0xE000E380` / `NVIC_NS_IABR0 0xE002E300`（SafeG L187-190） | asp3 **L198(`NVIC_VECTTBL`) の直後** | `#ifdef TOPPERS_SAFEG_M` | — | 低 |
| SCB 群: `SCB_AIRCR`/`_VECTKEY`/`_PRIS`/`_PRIGROUP_7`/`_SYSRESETREQS`/`_DIS_GROUP`、`SCB_SCR`/`_SLEEPDEEPS`、`SCB_UFSR`、`SCB_NS_VTOR 0xE002ED08`、`SCB_NS_SHCSR 0xE002ED24`（SafeG L216-231） | asp3 **L223(`CPACR_BASE`) の直後**（FPU 群の前） | `#ifdef TOPPERS_SAFEG_M` ブロック | — | 低 |
| `FPCCR_NS`（SafeG L238-240 = `0xE002EF34`） | asp3 **L232(`FPCCR_ADDR`) の直後** | `#ifdef TOPPERS_SAFEG_M` | **C1: `FPCCR_NS` ではなく `FPCCR_NS_ADDR` で定義**（M0 §11.4-C2 推奨）。SafeG コード側（C: launch_ns、§3⑩）の `FPCCR_NS` 参照を `FPCCR_NS_ADDR` へ書換え | 低 |
| `NSACR 0xE000ED8C` / `NSACR_FPU_ENABLE`（SafeG L248-251） | asp3 **L238(`FPCCR_LSPACT`) の直後**（FPU 群末尾） | `#ifdef TOPPERS_SAFEG_M` | — | 低 |
| SAU 群: `SAU_CTRL`/`_ALLNS`/`_ENABLE`、`SAU_TYPE`、`SAU_RNR`、`SAU_RBAR`、`SAU_RLAR`/`_LADDR_MASK`/`_NSC`/`_ENABLE`（SafeG L264-277）、`TT_RESP_S (1<<22)`（SafeG L279-282） | asp3 **L249(`FPCCR_INIT` の `#endif`) の直後**（ファイル末尾 `#endif ARM_M_H` の前） | `#ifdef TOPPERS_SAFEG_M` ブロック | — | 低 |

### 2.2 C1 規則（bare CPACR/FPCCR 禁止）の適用箇所
asp3_core arm_m.h は **既に C1 完了**（bare 無し・`CPACR_BASE`/`FPCCR_ADDR` のみ）。よって arm_m.h への bare 持ち込みは**禁止**。影響は **SafeG 由来 C コードの参照書換え**に出る:
- `core_kernel_impl.c`（§3）: SafeG L255 `CPACR` → `CPACR_BASE`、L256 `FPCCR` → `FPCCR_ADDR`。
- `launch_ns`（§3⑩）: SafeG L436 `FPCCR_NS` → `FPCCR_NS_ADDR`。
- `core_support.S` は CPACR/FPCCR を直接参照しない（grep 確認）ので C1 影響なし。

---

## 3. `core_kernel_impl.c`（launch_ns / SAU設定 / ITNS初期化 / AIRCR.PRIS）

asp3_core 版 392 行 / SafeG 版 441 行。共通 impl.c に載るのは **ITNS 全 NS 化 + AIRCR.PRIS + SCB_SCR Deep sleep 禁止 + NSACR + config_int の per-IRQ ITNS + launch_ns 本体**。**SAU リージョン設定は target_kernel_impl.c（分類D・per-board）にあり共通 impl.c には無い**（grep 実証: SAU_RBAR 等は target/*_gcc のみ）。よって M2 共通 impl.c に SAU 設定は載せない。

### 3.1 挿入マッピング表

| # | 内容(SafeG impl.c 実測) | asp3_core 挿入位置(関数/行) | #ifdef方針 | C1 | 難度 | リスク |
|--:|---|---|---|---|:--:|---|
| ⑧ | Deep sleep 禁止 `sil_wrw_mem(SCB_SCR, SCB_SCR_SLEEPDEEPS)`（SafeG L249-252） | `core_initialize()`、asp3 **L247(`sil_andw CCR_BASE`) と L249(`#ifdef TOPPERS_FPU_ENABLE`) の間** | `#ifdef TOPPERS_SAFEG_M` | — | 低 | — |
| ⑨ | FPU 有効時 NSACR 設定 `sil_wrw_mem(NSACR, NSACR_FPU_ENABLE)`（SafeG L257-259） | asp3 **L251(`sil_wrw_mem FPCCR_ADDR,FPCCR_INIT`) の直後、L252(`#endif FPU_ENABLE`) の前** | `#ifdef TOPPERS_SAFEG_M`（FPU_ENABLE ブロック内） | — | 低 | — |
| ⑩ | ITNS 全 NS 化 + AIRCR.PRIS（NS 割込優先度を下半分へ）（SafeG L273-280） | asp3 **L259(stk_top キャッシュ for ループ末尾) の直後、L260(`#else`) の前**＝`__TARGET_ARCH_THUMB>=4` ブロック内末尾 | `#ifdef TOPPERS_SAFEG_M` | — | 中 | `TMAX_INTNO` をループ上限に使用（SafeG L277）。imxrt600 と an505 で `TMAX_INTNO` 値が異なるが式は不変。 |
| ⑪ | config_int で当該 IRQ を Secure に戻す `sil_andw(NVIC_ITNS0+...)`（SafeG L332-338） | `config_int()`、asp3 **L313(`enable_int` の `}`) の直後、L314(`}` 関数末) の前** | `#ifdef TOPPERS_SAFEG_M` | — | 低 | `IRQNO_SYSTICK` 参照（chip/target で定義）。 |
| ⑫ | `launch_ns(intptr_t exinf)` ＋ `nonsecure_call_t` typedef（SafeG L419-441） | asp3 **L391(`default_int_handler` の `target_exit()`) 以降、L392(EOF) ＝ファイル末尾**に追加 | `#ifdef TOPPERS_SAFEG_M ... #endif` 独立ブロック | **C1: L436 `FPCCR_NS`→`FPCCR_NS_ADDR`** | 中 | 依存: `set_faultmask_ns`/`set_control_ns`/`set_msp_ns`（core_insn.h §4）、`SCB_NS_VTOR`/`FPCCR_NS_ADDR`/`FPCCR_INIT`（arm_m.h §2）、`cmse_nonsecure_call` 属性（要 `-mcmse`。CMake で SAFEG 時付与＝M1 plumbing 範囲か要確認）。 |

### 3.2 構造差の所見（impl.c）
- asp3_core `core_initialize` は **`set_exc_int_priority(EXCNO_DEBUG,0)`(asp3 L227)** を持つが SafeG に無い。これは asp3_core 由来の純増で SafeG 載せ替えと**無干渉**（DEBUG 例外優先度設定は SAFEG 経路に影響せず）。挿入アンカーは DEBUG 行より下流なので問題なし。**未確認**: SAFEG_M + DEBUG 例外優先度の相互作用は M2 実機で要観察（低リスク）。
- asp3_core は FPU 初期化に `CPACR_BASE`/`FPCCR_ADDR`(asp3 L250-251) を既使用＝C1 済。⑨の NSACR 挿入はその直後で C1 整合。
- **an505 と imxrt600 の SAU アドレス差**: 共通 impl.c の M2 対象外（SAU は target 側）。記録のみ: an505 = `SAU_RBAR 0x101FFE00`/region1 `0x00200000`/region2 `0x28200000`（target/mps2_an505_gcc/target_kernel_impl.c L125-139）、imxrt685 = `0x183FFE00`/`0x8400000`/`0x20240000`（target/mimxrt685evk_gcc/target_kernel_impl.c L292-303）。**SAU 設定値はボード固有のため分類D（SafeG-M repo 保持）で扱い、M2 共通載せ替えには含めない**。

---

## 4. `core_kernel_impl.h` / `core_insn.h` / `core_rename.{def,h}` / `core_unrename.h`

### 4.1 `core_kernel_impl.h`（IPM / IIPM_ENAALL の TZ 版）

| 内容(SafeG 実測) | asp3_core 挿入位置 | #ifdef方針 | 難度 | リスク |
|---|---|---|:--:|---|
| `INT_IPM`/`EXT_IPM` のシフト量を `7-TBITW_IPRI`/`8-TBITW_IPRI` で切替（SafeG L179-185） | asp3 **L179-180**（現状は `8-TBITW_IPRI` 固定の 2 行）を `#ifdef TOPPERS_SAFEG_M`(7) `#else`(8) `#endif` で**包む** | `#ifdef/#else` swap | 中 | **これは swap（既存 2 行を分岐化）**。背景: SAFEG では `IIPM_ENAALL=0x80` のため有効ビット幅が 1 減る→シフト量を 7 起点に。下の IIPM_ENAALL と**ペア**。 |
| `IIPM_ENAALL` を `0x80`(SAFEG)/`0`(非) で切替（SafeG L205-209） | asp3 **L200**（現状 `#define IIPM_ENAALL (0)` 単行）を `#ifdef TOPPERS_SAFEG_M`(0x80) `#else`(0) `#endif` で**包む** | `#ifdef/#else` swap | 中 | core_support.S の `movs r0,#IIPM_ENAALL`（複数）と意味が連動。③④(M1) の basepri 値も IIPM_ENAALL 依存。**最も波及が広い定義**。INT_IPM 切替とセットで入れる。 |

注: この 2 件は additive ではなく **swap**。M0 §4 は impl.h を「6 ifdef群」と記すが、TZ コア論理は上記 2 ペアが本質。残りは launch_ns/deactivate の extern 宣言（SafeG L828-835、§4.4 へ）。

### 4.2 `core_insn.h`（set_msp_ns / set_control_ns / set_faultmask_ns / is_secure）

| 内容(SafeG L177-215) | asp3_core 挿入位置 | #ifdef方針 | 難度 | リスク |
|---|---|---|:--:|---|
| `set_msp_ns`/`set_control_ns`/`set_faultmask_ns`（msr の NS バンク）＋ `is_secure`（`tt` 命令＋`TT_RESP_S`） | asp3 `core_insn.h` **L176(`#endif CORE_INSN_H` の前)＝ファイル末尾** | `#ifdef TOPPERS_SAFEG_M ... #endif` 独立追加 | 低 | 依存: `TT_RESP_S`(arm_m.h §2)。`is_secure` は SafeG コード内で実呼出し箇所要確認（grep では impl.c/launch_ns で未使用＝target 側か）。**未確認**: is_secure の利用者（M2 実装時に確認）。 |

### 4.3 `core_rename.def` / `core_rename.h` / `core_unrename.h`（識別子）

| 内容 | asp3_core 挿入位置 | 難度 | リスク |
|---|---|:--:|---|
| `launch_ns` / `deactivate_nonsecure_interrupts` の 3 点セット（rename.def に名前、rename.h に `#define X _kernel_X`、unrename.h に `#undef X`） | asp3 `core_rename.def` 末尾 / `core_rename.h` 末尾 / `core_unrename.h` 末尾 | 低 | `#ifdef` ガード可否要検討。**SafeG は rename 系を ifdef で囲っていない**（grep: SafeG rename.def/h/unrename に SAFEG ifdef 無し）。asp3_core では未定義シンボルの rename は無害（参照されないだけ）なので **ifdef 無しの素追加でも可**だが、M0 規律（全 SAFEG 由来に `【SAFEG】` 識別）に合わせ **`#ifdef TOPPERS_SAFEG_M` で囲む方が望ましい**。難度低。 |

### 4.4 launch_ns / deactivate の extern 宣言
SafeG `core_kernel_impl.h` L828-835 の `extern void deactivate_nonsecure_interrupts(void);` / `extern void launch_ns(intptr_t exinf);` を asp3_core `core_kernel_impl.h` 末尾へ `#ifdef TOPPERS_SAFEG_M` で追加。難度低。kernel_cfg（CRE_TSK の BTASK＝`deactivate_nonsecure_interrupts`、EXINF=`TOPPERS_NS_VTOR`）からの参照が解決される前提（cfg 断片は分類B、本書対象外）。

---

## 5. chip（imxrt600）

asp3_core に imxrt600 chip host **有**（`arch/arm_m_gcc/imxrt600/` 確認）。差は 2 マクロのみ。

| 内容 | SafeG 実測 | asp3_core 現状 | 挿入位置 | #ifdef方針 | 難度 |
|---|---|---|---|---|:--:|
| `TMIN_INTPRI` | `(-3)`(SAFEG) / `(-7)`(非)（chip_kernel.h L58-62） | `(-7)` 固定（chip_kernel.h **L58**） | 当該行を `#ifdef TOPPERS_SAFEG_M`(-3) `#else`(-7) `#endif` で包む | swap | 低 |
| `TBITW_IPRI` | `2`(SAFEG) / `3`(非)（chip_sil.h L60-64） | `3` 固定（chip_sil.h **L60**） | 当該行を `#ifdef TOPPERS_SAFEG_M`(2) `#else`(3) `#endif` で包む | swap | 低 |

背景: SAFEG は NS 用に優先度ビットを 1 本譲る（AIRCR.PRIS）ため Secure 側有効ビットが 3→2 に減り、最高優先度が -7→-3 に縮む。**§4.1 の IIPM_ENAALL=0x80 / INT_IPM シフト 7 と整合する**（TBITW_IPRI=2 → `7-2=5` シフト等）。chip と common impl.h は**必ず同一 SAFEG 設定で揃える**こと（不一致は割込優先度マスク破綻）。

---

## 6. 推奨載せ替え順序（M1 完了後）

1. **arm_m.h C2 純追加（§2）** — 全 asm/C サイトの依存シンボル土台。衝突なし・低リスク・最優先。
2. **chip(imxrt600) §5 ＋ core_kernel_impl.h §4.1** — TBITW_IPRI=2 / TMIN_INTPRI=-3 / IIPM_ENAALL=0x80 / INT_IPM シフトを**一括同時**投入（相互依存・揃えないと優先度破綻）。
3. **core_support.S ⑥（§1）＋ M1 ④swap の確認** — r6 生産(⑥)/消費(④) ペア。M1 成果と密結合のため早期に接続確認。
4. **core_support.S ①②④-FP ⑤（§1）** — FPU save/restore additive。★MVE 構造差に追従。⑤が最難（dispatch_btask_nofp）。
5. **core_insn.h §4.2 ＋ core_kernel_impl.c ⑧⑨⑩⑪ ＋ rename 系 §4.3/4.4** — C 側周辺。
6. **core_support.S ⑦（§1, deactivate 群）＋ core_kernel_impl.c ⑫(launch_ns)** — 最大の自己完結群。全依存（1〜5）充足後。最後。

理由: 依存方向（シンボル定義→参照）と「ペアで入れないと壊れる結合（②③④⑥、impl.h⇔chip、launch_ns⇔deactivate）」を優先。⑦⑫は依存先が全部揃ってからが安全。

---

## 7. DIVERGENCE_MAP 追記案（M0 §8 様式・記録は M2 実装時）

asp3_core `DIVERGENCE_MAP.md` の既存列（ファイル/ディレクトリ｜変更種別｜理由｜上流変更時のリスク｜担当レイヤ｜最終確認バージョン）に以下行を `【SAFEG】` 付きで追記する案。各 core_support.S サイトはソース内コメントに `/* 【SAFEG】#N */` を併記。

| ファイル / ディレクトリ | 変更種別 | 理由 | 上流変更時のリスク | 担当レイヤ | 最終確認バージョン |
|---|---|---|---|---|---|
| `arch/arm_m_gcc/common/core_support.S` | `#ifdef TOPPERS_SAFEG_M` additive 5 サイト(①②④-FP⑤⑥)＋自己完結群(⑦) | SafeG-M デュアル OS の BTASK FP 退避 skip と NS 割込デアクティベート | **★MVE 経路(L399-402/L465-468)変更時は⑤の vpush/vpop アンカー再確認必須。③④swap は M1 記載済** | arch【SAFEG】 | 3.7.2 |
| `arch/arm_m_gcc/common/arm_m.h` | C2 純追加(SAU/SCB_NS/SCB_AIRCR/SCB_SCR/SCB_UFSR/NSACR/ITNS/IABR/FPCCR_NS_ADDR/TT_RESP_S) | SafeG-M TZ レジスタ定義 | C1 規則(bare CPACR/FPCCR 禁止)維持。FPCCR_NS は FPCCR_NS_ADDR 命名 | arch【SAFEG】 | 3.7.2 |
| `arch/arm_m_gcc/common/core_kernel_impl.c` | `#ifdef` 取込み(SCB_SCR/NSACR/ITNS全NS化/AIRCR.PRIS/config_int ITNS/launch_ns) | デュアル OS 初期化・NS 起動 | DEBUG 例外優先度(asp3 純増)と非干渉。SAU は target 側(分類D) | arch【SAFEG】 | 3.7.2 |
| `arch/arm_m_gcc/common/core_kernel_impl.h` | `#ifdef/#else` swap(IIPM_ENAALL=0x80 / INT_IPM・EXT_IPM シフト 7) | NS へ優先度ビット 1 本譲る | chip(TBITW_IPRI) と必ず同期。波及最大 | arch【SAFEG】 | 3.7.2 |
| `arch/arm_m_gcc/common/core_insn.h` | `#ifdef` 追加(set_msp_ns/set_control_ns/set_faultmask_ns/is_secure) | NS バンクアクセス／TT 命令 | — | arch【SAFEG】 | 3.7.2 |
| `arch/arm_m_gcc/common/core_rename.{def,h}`,`core_unrename.h` | 識別子追加(launch_ns/deactivate_nonsecure_interrupts) | カーネル名前空間 | `#ifdef` ガード推奨 | arch【SAFEG】 | 3.7.2 |
| `arch/arm_m_gcc/imxrt600/chip_kernel.h`,`chip_sil.h` | `#ifdef/#else` swap(TMIN_INTPRI=-3 / TBITW_IPRI=2) | Secure 有効優先度ビット縮小 | common impl.h と同期必須 | chip【SAFEG】 | 3.7.2 |

---

## 8. 未確認事項（M2 実装着手時に確認）
- **U1**: `cmse_nonsecure_call` 属性に必要な `-mcmse` を SAFEG 時に CMake が付与するか（M1 plumbing 範囲か要確認）。launch_ns(§3⑫) のビルド前提。
- **U2**: `is_secure`（core_insn.h §4.2）の実呼出し箇所。共通 impl.c/launch_ns では未使用（grep）。target 側か未使用かを確認し、未使用なら追加是非を判断。
- **U3**: ★MVE 有効構成（M85 系）での BTASK FP skip 経路の実機検証。imxrt685(M33) は MVE 無のため当面影響なし。QEMU(an505=M33) も MVE 無。MVE 検証は将来のターゲット選定時。
- **U4**: M0 §5 の「⑧=例外サイト」は実測で独立サイトとして検出されず（本書では⑥do_dispatch 内に集約と判断）。M2 実装時に SafeG `core_support.S` 全 `#ifdef` を再 grep し齟齬がないか最終確認。
- **U5**: rename 系を `#ifdef TOPPERS_SAFEG_M` で囲むか素追加かの最終決定（§4.3）。M0 規律は識別子明示を要請するが asp3_core 既存 rename の体裁に合わせる。
