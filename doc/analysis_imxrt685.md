# TOPPERS/SafeG-M ソースコード解析 (i.MX RT685 / mimxrt685evk_gcc)

対象: SafeG-M 1.1.0 (ASP3 3.7.0 同梱) / NXP i.MX RT685 (Cortex-M33, chip=imxrt600, target=mimxrt685evk_gcc)

本書は SafeG-M の現状ソースコードを mimxrt685 ターゲットを軸に解析し，リファクタリング
(Step2〜4) の基礎情報としてまとめたものである。各記述には根拠となるファイルパスと行番号を
`file_path:line` 形式で付した。実機での動作確認は行っていないため，挙動の一部は「未確認」と明記する。

凡例:
- Secure 側 = ASP3 (`asp3/`), Non-secure 側 = FreeRTOS (`FreeRTOS/`)
- 主要コンパイルマクロ: `TOPPERS_SAFEG_M`, `TOPPERS_ENABLE_TRUSTZONE`

---

## 0. 全体構成と有効化フロー (前提)

SafeG-M は ARMv8-M TrustZone-M を用いた Dual-OS モニタである。1つの Cortex-M33 上で
Secure 側 ASP3 と Non-secure 側 FreeRTOS を分離して動かす。Non-secure 側は ASP3 から見ると
1本のタスク `_SAFEG_BTASK`(タスク ID = 1, 最低優先度 `TMAX_TPRI`, `TA_ACT`)として扱われる。

マクロの定義経路 (mimxrt685):
- `target/mimxrt685evk_gcc/Makefile.target:16-18` で `ENABLE_SAFEG_M` 未指定なら 1。
- `Makefile.target:34` で `ENABLE_TRUSTZONE = $(ENABLE_SAFEG_M)`。
- `arch/arm_m_gcc/common/Makefile.core:79-90` (CORE_TYPE=CORTEX_M33) で
  `ENABLE_TRUSTZONE=1` → `-DTOPPERS_ENABLE_TRUSTZONE`，`ENABLE_SAFEG_M=1` → `-DTOPPERS_SAFEG_M`。
- CORE_TYPE は `arch/arm_m_gcc/imxrt600/Makefile.chip:13` で `CORTEX_M33` 固定。

`TOPPERS_ENABLE_TRUSTZONE` は「TrustZone を有効化する Cortex-M33 一般」の差分(例外退避時の
EXC_RETURN・Secure 例外有効化)を指す。`TOPPERS_SAFEG_M` は「Dual-OS モニタとしての SafeG-M」の
差分(SAU/ITNS/launch_ns/BTASK 等)を指す。mimxrt685 では両者とも有効。

ASP3 単独で動かす場合: `configure.rb ... ENABLE_SAFEG_M=0`(`doc/user.txt:161-164`)。

---

## 1. Secure 側ブートと初期化フロー (リセット〜ASP3 起動)

### 1.1 ベクタテーブルとリセット
- ベクタテーブルはコンフィギュレータが `_kernel_vector_table[]` を生成する
  (`arch/arm_m_gcc/common/core_kernel.trb:116-166`)。`.vector` セクションに配置され，
  `[0]`=初期 MSP(=istack 末尾), `[1]`=`_kernel_start`(リセットハンドラ)。
- SafeG-M 有効時，例外 #6 (UsageFault) のベクタは通常の `_kernel_core_exc_entry` ではなく
  `_kernel_usagefault_handler` に差し替わる (`core_kernel.trb:124-126`)。これは Non-secure 割り込みの
  デアクティベート処理 (3.3 参照) に利用される特別扱い。
- #11 SVCall=`_kernel_svc_handler`, #14 PendSV=`_kernel_pendsv_handler` (`core_kernel.trb:142,146`)。
- リンカ上のベクタ配置・Flash 先頭は `target/mimxrt685evk_gcc/mimxrt685_s.ld:54-83`
  (`.boot_hdr` の後に `.text` 内 `KEEP(*(.vector))`)。

### 1.2 スタートアップ (`start_imxrt600.S`)
`_kernel_start` (`arch/arm_m_gcc/imxrt600/start_imxrt600.S:64-157`):
1. `cpsid f`(割込みロック) `start_imxrt600.S:65`
2. `INIT_MSP` 定義時に CONTROL=MSP, MSP=`_kernel_istkpt` 設定 `:67-77`
3. `hardware_init_hook` 呼出し `:85`
4. BSS 初期化 `:88-111`, DATA 初期化 `:115-141`
5. `software_init_hook` 呼出し `:151`
6. `sta_ker`(カーネル起動) `:154`

### 1.3 ハードウェア初期化 (`target_kernel_impl.c`)
`hardware_init_hook()` (`target/mimxrt685evk_gcc/target_kernel_impl.c:122-255`):
- RAM 上にコピーした `target_enter_fbb` を実行し forward body-bias モードへ
  (`:124-137`, 0x80001 へ分岐するコードバスアドレス利用)。
- PMIC(PCA9420) を I2C15 経由で SW1=1.15V に設定 `:139-174`。
- 水晶発振器→Main PLL を 300MHz に設定, main_clk 切替, FlexSPI/FRG クロック設定 `:184-230`。
- FlexSPI 外部メモリ領域 (0x0800_0000-0x1000_0000) をキャッシュ可(write-back)に設定し
  キャッシュ有効化 `:232-250`。
- シリアル用 IOCON P0_1/P0_2 設定 `:251-254`。

`software_init_hook()` `:257-264`: SIO 初期化(`TOPPERS_OMIT_TECS` 時 `sio_initialize`/`sio_opn_por`)。

### 1.4 ターゲット依存初期化 (`target_initialize`) — SAU 設定を含む
`target_initialize()` (`target_kernel_impl.c:277-307`):
1. `core_initialize()` 呼出し(1.5) `:282`
2. (非 TECS 時) `tPutLogSIOPort_initialize()` `:287`
3. **SAU 設定** (`TOPPERS_SAFEG_M` 時) `:290-306`:
   - リージョン0 = NSC: `0x183FFE00`〜`0x183FFFFF`, `SAU_RLAR_NSC|ENABLE` (`:292-294`)
   - リージョン1 = NS Flash: `0x8400000`〜`0x87FFFFF`, ENABLE (`:296-298`)
   - リージョン2 = NS RAM: `0x20240000`〜`0x2047FFFF`, ENABLE (`:300-302`)
   - `SAU_CTRL_ENABLE` 後に `isb`/`dsb` (`:303-305`)
   - SAU レジスタ定義は `arm_m.h:264-283` (SAU_CTRL/RNR/RBAR/RLAR, RLAR_NSC ビット等)。

### 1.5 コア依存初期化 (`core_initialize`) — AIRCR.PRIS / BASEPRI / ITNS
`core_initialize()` (`arch/arm_m_gcc/common/core_kernel_impl.c:199-281`):
- CPU ロックフラグ初期化 `lock_cpu_dsp()` `:206`(BASEPRI=IIPM_LOCK, saved_iipm=IIPM_ENAALL)。
- ベクタテーブルアドレスを VTOR に設定 `:215`。
- 各例外優先度を 0 (BASEPRI でマスク不可) に設定 `:223-228`。PendSV のみ最低優先度
  `INT_IPM(-1)` `:228`。`TOPPERS_ENABLE_TRUSTZONE` 時 SecureFault(#7) も優先度 0 `:230`。
- MPU/Bus/Usage 例外を有効化 `:236-238`。TrustZone 時 SecureFault 有効化 `:240`。
- CCR.STKALIGN を 0(4byte アライン) `:247`。
- **SafeG-M 時**: `SCB_SCR=SLEEPDEEPS` で Non-secure からの Deep sleep を禁止 `:249-252`。
- FPU 有効時: CPACR/FPCCR 設定, SafeG-M 時 `NSACR=NSACR_FPU_ENABLE` で NS の FPU 許可 `:254-260`。
- **SafeG-M 時 (重要)** `:273-280`:
  - `SCB_AIRCR = SCB_AIRCR_DIS_GROUP`(= VECTKEY|PRIS|SYSRESETREQS, `arm_m.h:225`)を書き込み，
    **AIRCR.PRIS=1** で Non-secure 例外優先度を 0x80 以下に強制 `:275`。
  - 一旦すべての割り込みを Non-secure に割り当て: `NVIC_ITNS0`(0xE000E380, `arm_m.h:188`)を
    全ビット 1 に `:277-279`。以降 `config_int()`(2.4/3.5)で Secure 用のみ 0 に落とす。

設計判断(`doc/design.txt:7-27`): AIRCR.PRIS=1 で NS 例外を 0x80 以下に強制し，Secure 例外が
マスクされないよう「例外優先度の最上位ビットを常に 0」にする方式1を採用(プリエンプト優先)。

---

## 2. TrustZone/SafeG 拡張の実装箇所

### 2.1 レジスタ/定数定義 (`arm_m.h`)
`TOPPERS_SAFEG_M` ガードで以下が追加される:
- `NVIC_ITNS0`(0xE000E380), `NVIC_NS_IABR0`(0xE002E300) `arm_m.h:187-190`
- SCB: `SCB_AIRCR`/`PRIS`(bit14)/`DIS_GROUP`, `SCB_SCR_SLEEPDEEPS`, `SCB_UFSR`,
  `SCB_NS_VTOR`(0xE002ED08), `SCB_NS_SHCSR`(0xE002ED24) `arm_m.h:216-231`
- `FPCCR_NS`(0xE002EF34) `arm_m.h:238-240`, `NSACR`/`NSACR_FPU_ENABLE` `arm_m.h:248-251`
- SAU 一式 `arm_m.h:264-283`, `TT_RESP_S`(bit22) `arm_m.h:282`
- `TOPPERS_ENABLE_TRUSTZONE` 側: `EXC_RETURN_S`(0x40), `EXC_RETURN_NESTED`(0xFFFFFFF1)
  `arm_m.h:74-77`。また `EXC_RETURN` のデフォルト値が TrustZone 有無で分岐 `arm_m.h:63-67`。

### 2.2 特殊命令インライン (`core_insn.h`)
`TOPPERS_SAFEG_M` 時 `core_insn.h:177-215`:
- `set_msp_ns()` (`msr msp_ns`), `set_control_ns()` (`msr control_ns`),
  `set_faultmask_ns()` (`msr faultmask_ns`) `:181-200`
- `is_secure(addr)`: `tt` 命令 + `TT_RESP_S` でアドレスのセキュリティ属性判定 `:205-214`

### 2.3 割込み優先度モデルの変更 (`core_kernel_impl.h`, `chip_*.h`)
SafeG-M はビット幅を1減らし最上位ビットを 0 固定する設計のため，優先度モデルが変わる:
- `IIPM_ENAALL`: 通常 0 → SafeG-M では **0x80** `core_kernel_impl.h:205-209`。
  これは「全割り込み許可」状態でも BASEPRI=0x80 を保ち，NS 割り込み(0x80以下)を禁止することを意味する。
- `EXT_IPM`/`INT_IPM` マクロのシフト量が `7 - TBITW_IPRI`(SafeG-M)/`8 - TBITW_IPRI`(通常)で分岐
  `core_kernel_impl.h:179-185`。
- `TBITW_IPRI`: SafeG-M 時 2, 通常 3 (`arch/arm_m_gcc/imxrt600/chip_sil.h:61-63`)。
- `TMIN_INTPRI`: SafeG-M 時 -3, 通常 -7 (`arch/arm_m_gcc/imxrt600/chip_kernel.h:58-62`)。

つまり SafeG-M 有効化で Secure 側が使える割込み優先度段数が半減する。

### 2.4 SafeG-M で変わる主な関数 (`core_kernel_impl.c`)
- `config_int()` `:305-339`: 通常の優先度設定後，SafeG-M 時 `intno > IRQNO_SYSTICK` の
  割り込みを ITNS で **Secure** に変更 `:332-338`。→ `CFG_INT()` で設定した割込みのみ Secure。
- `launch_ns(exinf)` `:419-440`: Non-secure 起動ルーチン(C)。下記 3.1。

---

## 3. Secure ⇔ Non-secure 連携

### 3.1 Non-secure 起動 (`launch_ns`)
`launch_ns(intptr_t exinf)` (`core_kernel_impl.c:421-440`, exinf=NS ベクタテーブル先頭=`TOPPERS_NS_VTOR`):
1. `set_faultmask_ns(1)`: NS 割り込みを禁止 `:423`
2. `set_control_ns(0)`: 特権 + MSP 使用 `:424`
3. `set_msp_ns(*exinf)`: NS スタックポインタ初期値(ベクタ[0]) `:425`
4. `SCB_NS_VTOR = exinf`: NS ベクタテーブルオフセット設定 `:426`
5. `FPCCR_NS = FPCCR_INIT`: NS Lazy stacking の LSPACT をクリア(無効スタックへの退避防止) `:427-436`
6. `set_basepri(0)`: NS 割り込みを通すため BASEPRI を 0 に `:437`
7. `entry = *(exinf+4)` を `cmse_nonsecure_call` で呼出し(ベクタ[1]=NS リセットハンドラ) `:438-439`

NS 起動時の保証状態(CONTROL=0, MSP=ベクタ先頭値, FAULTMASK=1, FPCCR=Secure 初期値)は
`doc/user.txt:123-132` に対応する。

### 3.2 BTASK のディスパッチ経路
`_SAFEG_BTASK`(ID=1)はコンフィグで生成される:
- `arch/arm_m_gcc/common/core_kernel.cfg:7`:
  `CRE_TSK(_SAFEG_BTASK, { TA_ACT, TOPPERS_NS_VTOR, _kernel_deactivate_nonsecure_interrupts, TMAX_TPRI, DEFAULT_ISTKSZ, NULL });`
  → タスク本体が `deactivate_nonsecure_interrupts`, exinf が NS ベクタ先頭。
- これは `target_kernel.cfg:7-9` から SafeG-M 時のみ INCLUDE される。
- コンフィギュレータは BTASK の ID が 1 であることを静的検査する
  (`core_kernel.trb:217-223`, 不一致なら `#error`)。
- 生成例: `build/kernel_cfg.c:45`(TA_ACT, exinf=TOPPERS_NS_VTOR, task=deactivate_nonsecure_interrupts)。

ディスパッチャ (`core_support.S`) は「実行タスクが BTASK か否か」を `_kernel_tcb_table`(=ID1 の
TCB 先頭)との比較で判定し，BASEPRI を切り替える:
- PendSV `pendsv_handler` `:212-221`: `p_runtsk == _kernel_tcb_table` なら BASEPRI=0(NS 割込み許可),
  そうでなければ `IIPM_ENAALL`(=0x80, NS 割込み禁止)。
- `do_dispatch`/`dispatcher_0` `:428-435`: 同判定で r6 に BASEPRI 値(0 or IIPM_ENAALL)を用意し
  SVC 経由で復帰。
- `svc_handler` `:305-310`: SafeG-M 時 `msr basepri, r6`(BTASK なら 0, 他は IIPM_ENAALL)。

BTASK が選択されると本体 `deactivate_nonsecure_interrupts` が走り(3.3)，最終的に `launch_ns` で
NS へ遷移する。

### 3.3 NS 割り込みのデアクティベート (`deactivate_nonsecure_interrupts`)
`core_support.S:622-727` (`TOPPERS_SAFEG_M` 時のみ)。BTASK が実行状態に移行する際，前回 NS で
アクティブのまま残った割り込みを完了させてから `launch_ns` へ分岐する複雑な状態機械:
- `SCB_NS_SHCSR=0` で NS 例外をデアクティベート `:626-628`。
- `NVIC_NS_IABR0`(NS active bit) を 0〜15 番レジスタまで走査 `:629-704`。
- アクティブな NS 割り込みを発見すると，意図的に `udf`(UsageFault)を発生させ Handler モードへ移行
  (`deactivate_first_entry`/`deactivate_udef_entry` `:706-714`)。
- UsageFault ハンドラ `usagefault_handler` `:558-597` が「意図的な UsageFault」を判定
  (EXC_RETURN が Secure/Thread/PSP かつ戻り先が `deactivate_udef_entry`)し，ダミー例外フレームを
  積んで `EXC_RETURN_NESTED` で `deactivate_exc_point` へ例外リターンさせる。
- これにより各 NS 割り込みハンドラを順に完了させ，処理後 ITNS を元(Non-secure)に戻して
  最後に `launch_ns` へ `:717-720`。
- 実行中の優先度はほぼ常に 0x80 (UsageFault 中を除く)で，Secure 割り込みでプリエンプト可能 `:606-607`。

備考: `usagefault_handler` は最高優先度(#6 優先度 0)で動くため，通常の UsageFault と
意図的なものを区別し，通常時は `core_exc_entry` へ分岐する `:573-580`。

### 3.4 Secure gate (NS → Secure 呼出し)
- Secure 側関数に `__attribute__((cmse_nonsecure_entry))` を付けると NS から呼べる。
  例: `sample/sample1.c:547 nonsecure_task(int n)`, `:598 nonsecure_message(const char *str)`。
- これらは ASP3 API(`slp_tsk`, `ext_tsk`, `dly_tsk`, `syslog` 等)を発行可能 `:556-594, 600`。
- **戻る前に `set_basepri(0)` が必須** `sample1.c:595, 601`。これがないと NS 割り込みが入らない
  (`doc/user.txt:134-140`, `doc/design.txt:45-53` の BASEPRI_S 方式の代償)。
- リンク時 `-Wl,--cmse-implib --out-implib=.../secure_nsclib.o` で NS 用インポートライブラリを生成
  (`Makefile.target:112`)。Secure gate veneer は `.gnu.sgstubs` に置かれ NSC リージョン
  `0x183FFE00`(`mimxrt685_s.ld:5,14-22`) に配置される。

### 3.5 割込みの NS 割り当て (ITNS)
- 既定: `core_initialize` で全割込みを NS に(`core_kernel_impl.c:277-279`)。
- `CFG_INT()` で設定した割込みのみ `config_int()` が ITNS を 0 にして Secure 化
  (`core_kernel_impl.c:332-338`)。→ user.txt 3.5(`doc/user.txt:152-159`) と整合。
- 「Secure 動作中は NS 全割込みがマスクされる」(`doc/user.txt:117-121`)のは BASEPRI=0x80 による。

---

## 4. メモリ配置 (Flash/RAM, SAU, リンカ)

### 4.1 Secure 側リンカ (`mimxrt685_s.ld`)
MEMORY (`target/mimxrt685evk_gcc/mimxrt685_s.ld:1-8`):
- `QSPI_FLASH (rx)`: ORIGIN `0x18000000`, LENGTH `0x3FFE00`(約4M, Secure Flash)
- `SG_veneer_table (rx)`: ORIGIN `0x183FFE00`, LENGTH `0x200`(512B, Secure gate veneer = NSC)
- `SRAM (rwx)`: ORIGIN `0x30000000`, LENGTH `0x240000`(2.25M, Secure SRAM)
- `USB_RAM (rwx)`: ORIGIN `0x50140000`, LENGTH `0x4000`(16K)

セクション要点:
- `.gnu.sgstubs`(Secure gateway veneer)を `SG_veneer_table` に配置 `:14-22`。
- `.boot_hdr`(外部 Flash ブート用 IVT/flash_conf, `KEEP(*(.flash_conf))`) `:41-51`。
- `.text` 先頭に `KEEP(*(.vector))` + Global Section Table(data/bss テーブル) `:54-83`。
- `*(.target_enter_fbb)` と `__target_enter_fbb_end` `:81-82`(1.3 のコピー処理が参照)。
- `ENTRY(_kernel_start)` `:10`。

### 4.2 Non-secure 側リンカ (`standalone.ld`) と SAU の対応
MEMORY (`FreeRTOS/sample/mimxrt685evk_gcc/standalone.ld:4-6`):
- NS Flash (QSPI): ORIGIN `0x8400000`, LENGTH `0x400000`(4M) — Secure SAU NS Flash と一致。
- NS SRAM: ORIGIN `0x20240000`, LENGTH `0x240000`(2.25M) — Secure SAU NS RAM と一致。
- NS USB_RAM: ORIGIN `0x40140000`, LENGTH `0x4000`(16K)。
- `.isr_vector` を NS Flash 先頭 `0x8400000` に配置(`standalone.ld:21-22`)。
  → `TOPPERS_NS_VTOR=0x8400000`(`Makefile.target:110`)と一致し，launch_ns が `SCB_NS_VTOR` に設定
  (`core_kernel_impl.c:426`)。
- NS スタック 0x2000 を SRAM 末尾に配置(`standalone.ld:188-200`), ヒープ 0x2000(`:179-186`)。
- 注: `standalone.ld:13` の `ENTRY` は `_kernel_start` だが，実際の NS エントリは
  startup.c の `ResetISR`(`Makefile:75` `ENTRY_RTOSDemo=ResetISR`)。ENTRY シンボル名は流用と推測。

整合性: Secure 側 SAU NS リージョン(`target_kernel_impl.c:297-302`)= NS リンカの Flash/RAM 領域。

### 4.3 Makefile.target の TrustZone 関連
`Makefile.target`:
- `LDSCRIPT`: ENABLE_SAFEG_M=1 で `mimxrt685_s.ld`, 0 で `mimxrt685.ld` `:100-104`。
- ENABLE_SAFEG_M=1 時 `:106-114`: `-DTOPPERS_NS_VTOR=0x8400000`, `-mcmse`,
  `--cmse-implib --out-implib=.../FreeRTOS/sample/$(TARGET)/secure_nsclib.o`。
- KERNEL_TIMER=TIM(`:29`)→ `-DUSE_TIM_AS_HRT`, `target_timer.o`(`:70-73`)。SYSTICK ではなく
  OS Event Timer 系を高分解能タイマに使用(`target_timer.c`)。
- FPU: `FPU_USAGE=FPU_LAZYSTACKING`(`:23`)→ `TOPPERS_FPU_CONTEXT` 等(`Makefile.core:156-158`)。

---

## 5. ビルド構成

### 5.1 configure.rb の役割
`asp3/configure.rb`: ASP3 標準の Makefile 生成スクリプト。`-T <target>` 必須 `:137-139, 272-286`。
テンプレート(`sample/Makefile`, `:264`)中の `@(VAR)` を変数表で置換し `Makefile` を生成 `:318-345`。
SafeG-M 固有処理は無く，`-O "ENABLE_SAFEG_M=0"` 等の変数は `$vartable` 経由で渡る `:221-227`。

### 5.2 Makefile 階層 (Secure)
- 生成 `Makefile`(テンプレート=`sample/Makefile`) → `Makefile.target`(ボード) →
  `include Makefile.chip`(`Makefile.target:119`) → `include Makefile.core`(`Makefile.chip:39`)。
- `Makefile.core` がコア種別(CORTEX_M33→ARMV8M, `__TARGET_ARCH_THUMB=5`)・FPU・TrustZone マクロを決定
  (`Makefile.core:79-159`)。`core_support.o` をリンク `:115`。
- ボード固有オブジェクト: `start_imxrt600.o`, `target_kernel_impl.o`, `flash_config.o`,
  `target_timer.o`(`Makefile.target:54-73`)。

### 5.3 Non-secure (FreeRTOS) ビルド
- `FreeRTOS/sample/mimxrt685evk_gcc/Makefile` が共通定義 `FreeRTOS/sample/makedefs` を include。
  GCC ポートは ARM_CM33_NTZ/non_secure(`Makefile:31,33`, NTZ=カーネル自体は非 TrustZone)。
- FreeRTOS コア(tasks/queue/timers/port/portasm/heap_4)と `startup.o`(エントリ `ResetISR`)を
  リンク(`Makefile:35-49,75`)。
- `makedefs:93` の LDFLAGS が `secure_nsclib.o` を直接リンク対象に含める(CMSE インポートライブラリ)。
- ツールチェーン `arm-none-eabi-gcc`, `-mcpu=cortex-m33 -mthumb`(`makedefs:63,69,88`)。

### 5.4 Secure → Non-secure のビルド依存
- 必ず Secure(ASP3)を先にビルド(`doc/user.txt:101-103`)。理由: Secure リンク時に生成される
  `secure_nsclib.o`(CMSE インポートライブラリ)を NS 側リンクが参照するため
  (`Makefile.target:112` → `makedefs:93`)。
- 本ツリーには `FreeRTOS/sample/mimxrt685evk_gcc/secure_nsclib.o` が既に存在。

---

## 6. Non-secure 側 (FreeRTOS) の詳細

### 6.1 起動 (`startup.c` ResetISR)
`FreeRTOS/sample/mimxrt685evk_gcc/startup.c`:
- ベクタテーブル `g_pfnVectors[]`(`.isr_vector`, `:248-335`)。`[0]`=`_vStackTop`, `[1]`=`ResetISR`,
  `[8]`=CMSE 機能チェック。
- `ResetISR`(`:384-444`): 割込み禁止(`cpsid i` `:387`)→ data コピー(`:405-410`)→ bss ゼロ
  (`:414-418`)→ C++ 初期化(`:425`)→ 割込み許可(`cpsie i` `:429`)→ `main()` 呼出し(`:435`)。
- NS 起動時の保証状態(CONTROL=0, MSP=ベクタ[0], FAULTMASK=1, FPCCR=Secure 初期値)は
  `doc/user.txt:125-132`。

### 6.2 Secure gate の利用
- NS 側 `FreeRTOS/sample/main.c`: `extern void nonsecure_task(int n);`(`:7`)を FreeRTOS タスクから
  `nonsecure_task(++n)`(`:13`)で呼出し。
- リンク時 `secure_nsclib.o` のスタブが BLXNS(NS→Secure 遷移)に解決され，Secure 側
  `sample1.c:547`(`nonsecure_task`)/`:598`(`nonsecure_message`)へ到達する。

### 6.3 FreeRTOSConfig.h の要点
`FreeRTOS/sample/mimxrt685evk_gcc/FreeRTOSConfig.h`:
- `configENABLE_TRUSTZONE = 0`(`:52-54`): NS 自身は TrustZone を管理しない(管理は Secure ASP3 側)。
- `configRUN_FREERTOS_SECURE_ONLY = 0`(`:55-57`): NS 側で動作。
- `configENABLE_FPU = 0`(`:46-48`), `configENABLE_MPU = 0`(`:49-51`)。
- `configCPU_CLOCK_HZ = 300000000`(`:61`), `configTICK_RATE_HZ = 200`(`:62`),
  `configMAX_PRIORITIES = 5`(`:63`)。
- SVC/PendSV/SysTick ハンドラを CMSIS 名にマップ(`:155-157`)。

---

## 7. safeg.patch 群の要約 (Step3/4 の基礎情報)

各 `safeg.patch` は **vanilla ASP3 に対する SafeG-M 差分の記録**で，ツリーには適用済み・
自動適用スクリプトは無い(`CLAUDE.md` Step3)。重要な観察: **すべての差分は arm_m 移植層/
チップ/ターゲット/サンプルに閉じており，ASP3 のアーキテクチャ非依存(コア)部には及んでいない。**

- **arch/arm_m_gcc/common/safeg.patch** (深いコア差分): Makefile.core, arm_m.h, core_insn.h,
  core_kernel.cfg, core_kernel.trb, core_kernel_impl.{c,h}, core_rename/unrename, core_support.S を変更。
  TrustZone レジスタ定義, NS 操作インライン(`set_*_ns`, `is_secure`), 優先度モデル変更
  (IIPM_ENAALL=0x80, シフト量), SAU/ITNS/AIRCR 初期化, `launch_ns`, UsageFault ベクタ差し替え,
  `deactivate_nonsecure_interrupts`(約176行のアセンブリ), SVC/PendSV の BASEPRI 切替を追加。
  → リファクタリングで最も重い部分。
- **arch/arm_m_gcc/imxrt600/safeg.patch** (チップ): chip_kernel.h(`TMIN_INTPRI -7→-3`),
  chip_sil.h(`TBITW_IPRI 3→2`)。優先度ビット幅半減のみ。lpc5500/stm32l5xx も同内容。
- **target/mimxrt685evk_gcc/safeg.patch** (ターゲット, 主経路): Makefile.target(ENABLE_SAFEG_M/
  TRUSTZONE, -mcmse, NS_VTOR, cmse-implib), `mimxrt685_s.ld`(Secure 用リンカ新規),
  target_kernel.cfg(core_kernel.cfg を INCLUDE), target_kernel_impl.c(SAU 3 リージョン設定)。
  → 局所化しやすい。lpc55s69/nucleo_l552 も同パターン(後者は GTZC/MPCBB 追加)。
- **sample/safeg.patch** (アプリ): Makefile(ENABLE_SAFEG_M 受け渡し), sample1.c(menu '4' で
  `_SAFEG_BTASK` へ切替, `nonsecure_task`/`nonsecure_message` の cmse_nonsecure_entry 追加)。
  → 局所化容易。

リファクタリング論点(Step3/4 への示唆):
- **コア差分の扱い**: common/safeg.patch は ASP3 の arm_m 移植層そのものを書き換えており，
  「素の ASP3 へのパッチ依存解消」には (a) 移植層を SafeG-M 側に丸ごと持つ，または
  (b) 優先度モデル/ディスパッチャ差分を `#ifdef TOPPERS_SAFEG_M` のまま上流 ASP3 に取り込む，
  のいずれかが必要。アーキ非依存部に差分が無い点は有利。
- **チップ差分の最小化**: 優先度マクロ(TBITW_IPRI/TMIN_INTPRI)の2値分岐のみなので，config 化
  すれば3チップ分のパッチを1か所に集約できる。

---

## 8. 解析中に気づいた疑問点・リファクタリング論点

1. **SRAM アドレス系の複数性**(4.1): Secure リンカは `0x30000000`(命令バスエイリアス)を使う一方，
   `hardware_init_hook` は `0x20080000`(データバス)へコピーし `0x80001`(コードバス)へ分岐する。
   SAU NS RAM は `0x20240000` 系。この複数系統(0x30000000/0x20000000/0x08000000 エイリアス)の
   使い分けの正当性は未確認で，ドキュメント化が望ましい。
2. **`deactivate_nonsecure_interrupts` の複雑さ**: UsageFault を意図的に発生させる状態機械
   (`core_support.S:622-727`)は SafeG-M 最大の難所。最高優先度の `usagefault_handler` を専有する
   ため，アプリが UsageFault を使えない/競合する懸念。Step2 の ASP3 更新時に最も壊れやすい箇所。
3. **BTASK の ID 固定(=1)依存**: ディスパッチャが `_kernel_tcb_table`(先頭=ID1)との比較で
   NS 判定する(`core_support.S:214,429`)ため，BTASK は必ず ID1 でなければならず，
   コンフィギュレータが `#error` で強制(`core_kernel.trb:217-223`)。柔軟性に欠ける設計。
4. **BASEPRI_S 方式の副作用**: 全 Secure gate 末尾に `set_basepri(0)` が必要
   (`sample1.c:595,601`)。付け忘れると NS 割込みが止まる。設計判断は `doc/design.txt:45-55`。
   gate 自動生成やラッパで強制する仕組みがあると安全。
5. **優先度段数の半減**: SafeG-M 有効化で TBITW_IPRI が 3→2 になり Secure 側の割込み優先度段数が
   半減。アプリ側の優先度設計に影響するため明示が必要。
6. **`SCB_SCR` の二重定義**: `target_kernel_impl.c:52-56` で SafeG-M 時に `arm_m.h` の `SCB_SCR` を
   `#undef` し別定義している。マクロ衝突回避のための応急処置で，整理対象。
7. **NS リンカの ENTRY 不一致**: `standalone.ld:13` の `ENTRY(_kernel_start)` は Secure 用シンボルの
   流用と思われ，実エントリ `ResetISR`(`Makefile:75`)と異なる。混乱の元で要整理。
8. **timer**: 既定 `KERNEL_TIMER=TIM`(OS Event Timer)。TIM 割込みが Secure 固定か
   (CFG_INT 経由で ITNS=Secure 化されるか)は未確認。
9. **FreeRTOS 同梱の扱い (Step4)**: NS 側は FreeRTOS を丸ごと同梱し，リンク時に `secure_nsclib.o`
   のみで Secure と結合する疎結合。Secure 側 common/safeg.patch ほど深い改変は無いとみられ，
   外部 FreeRTOS への切替は Secure 側より容易と推測される(詳細差分は本書 7 章の範囲外, 未確認)。

---

## 追記: BTASK FPU コンテキスト退避バグの修正 (修正A)

`core_support.S` のディスパッチャ(PendSV/SVC/do_dispatch)の FPU コンテキスト退避・復帰で，
対象が BTASK(`p_runtsk==_kernel_tcb_table`) のときは退避/復帰を対称にスキップするよう修正した
(`#ifdef TOPPERS_SAFEG_M` ガード, FPU 有効時のみ)。
理由: NS→Secure gate の GCC 生成 veneer が NS 復帰前に FP スクラッチをゼロ化して
CONTROL_S.FPCA=1 を残すため，BTASK 横取りディスパッチ時の `vstmdb {s16-s31}` が
BTASK の Secure スタックを超え PSPLIM_S 違反(HardFault, PC≈0x180026c0)となっていた。
実機(mimxrt685)で A3/C1/C2/C3 が PASS し，既定ビルドの DONE 後クラッシュも解消することを確認。
詳細・検証ログは doc/test_safeg.md 「修正(A) 適用結果」を参照。
