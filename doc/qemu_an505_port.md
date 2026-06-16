# SafeG-M を QEMU mps2-an505 で動かす移植設計メモ

対象: SafeG-M(ASP3 3.7.0 同梱) を QEMU `mps2-an505`(Cortex-M33 IoTKit, FPU有, TrustZone) へ移植し、
遷移テスト `test/` 一式を実機なしで回す（CI化）。本書は実コードのビルド接続部を調査して得た
移植の道筋。`file:line` は調査時点の根拠。

## 0. 結論（実現性）
**○ 現実的。想定よりも負担が小さい。** 理由:
- SafeG-M の `arch/arm_m_gcc/common` に **汎用 `start.S` と SysTick タイマ `core_timer.c` が既存**で、
  `KERNEL_TIMER = SYSTICK` も対応済み（`target/mimxrt685evk_gcc/Makefile.target:66-68`）。
  → an505 では imxrt600 固有の `start_imxrt600.S`/FlexSPI/PMIC/PLL/cache 初期化は**不要**。
- QEMU 8.2.2 `mps2-an505 -cpu cortex-m33` は FPU(fpv5-sp)・TrustZone(SAU/ITNS/SG)・遅延スタッキング・
  **スタックリミット(STKOF)忠実**（別調査で実証、`doc/test_safeg.md` 参照）。
- ベースの board ロジックは ASP3CORE `target/mps2_an505_gcc/` から流用可
  （`mps2_an505.ld`, `cmsdk_uart.c`, `target_serial.c`, `target_timer.c`, `target_kernel_impl.c`）。

唯一の新規実装は **(a) an505 用 `target_kernel_impl.c`(SAU/clock)**、**(b) CMSDK-UART SIO**、
**(c) リンカスクリプト**、**(d) 薄い chip 層**、**(e) cfg/trb 一式**。いずれも既存資産の組替えが中心。

## 1. ビルド系の違い（最重要の論点）
| | SafeG-M | ASP3CORE an505 |
|---|---|---|
| ビルド | Makefile + `configure.rb`(Ruby) | CMake + presets |
| コンフィギュレータ | TECS(tecsgen) + `.trb` | 非TECS(`TOPPERS_OMIT_TECS`) + `.py` |
| SIO | TECS `tSIOPortTarget`(FLEXCOMM, chip層) | **非TECS** `cmsdk_uart.c`+`target_serial.c` |
| arm_m_gcc/common | SafeG拡張あり(別版) | TZ層なし(別版) |

→ **an505 は SafeG-M の Makefile/configure.rb 体系で新規ターゲットとして作る**。ASP3CORE のソースは
ロジックを移植するが、ビルド glue(CMake/py)は使わない。

### SIO の2方式（要決定）
- **方式S1（推奨・低工数）: 非TECSビルド**。`configure.rb -w`(TECS不使用) で、ASP3CORE の
  `cmsdk_uart.c`+`target_serial.c`(非TECS SIO)をそのまま移植。
  - 前提確認が必要: **SafeG-M 同梱 asp3 の syssvc が非TECS syslog/logtask を持つか**
    （本ツリーの `asp3/syssvc/` は TECS コンポーネント中心。非TECS版 `syslog.c`/`logtask.c` の有無を要確認）。
    無ければ ASP3CORE もしくは素の ASP3 3.7 から非TECS syssvc を持ち込む。
- **方式S2: TECS のまま**。CMSDK-UART を SafeG-M の `tSIOPortTarget` 互換 TECS コンポーネントとして実装
  （`target/mimxrt685evk_gcc/tSIOPortTarget.cdl` + `tSIOPortTargetMain_inline.h` を雛形に、
  FLEXCOMM レジスタアクセスを CMSDK UART に置換）。テスト一式が既に TECS ビルドで動くので統合は楚だが、
  TECS SIO コンポーネントの新規記述が要る。
- 判断: まず S1 の前提（非TECS syssvc 有無）を確認。あれば S1、無ければ S2。

## 2. 再利用できる SafeG-M 既存資産（file:line）
- 起動: `asp3/arch/arm_m_gcc/common/start.S`（`_kernel_start`, INIT_MSP, BSS/DATA 初期化, hardware/software_init_hook 呼出）。
- タイマ: `asp3/arch/arm_m_gcc/common/core_timer.{c,cfg,h}`（SysTick）。`Makefile.target` で `KERNEL_TIMER=SYSTICK`
  → `-DUSE_SYSTICK_AS_TIMETICK` + `core_timer.o`（`mimxrt685/Makefile.target:66-69`）。
- M33/TZ コア: `Makefile.core` が CORE_TYPE=CORTEX_M33 で `core_support.o`/`core_kernel_impl.o` と
  `-DTOPPERS_CORTEX_M33`/`-DTOPPERS_ENABLE_TRUSTZONE`/`-DTOPPERS_SAFEG_M`/FPUフラグを自動付与
  （`Makefile.core:79-90,144-158`）。**FP退避の修正(A)は `core_support.S` に適用済み**＝an505でも有効。
- SafeG 本体: `core_kernel_impl.c:421 launch_ns`, `core_support.S:622 deactivate_nonsecure_interrupts`,
  `arm_m.h`(SAU/ITNS/SCB_NS/FPCCR_NS)。SAU 設定処理は **mimxrt685 の `target_kernel_impl.c` の
  `target_initialize()`** を an505 アドレスに合わせて移植。
- テスト: `test/` 一式（AI出力 `[TST]`、ハーネス、gate、NSベアメタル）。CI終了は ASP3CORE 流の
  **半ホスティング SYS_EXIT**（`target_exit` で `bkpt 0xab`、`-semihosting-config enable=on`）を採用すると
  「1コマンドで pass/fail」化しやすい（`test/common` の DONE 出力後に ext_ker→target_exit→SYS_EXIT）。

## 3. ASP3CORE an505 から移植する board ロジック
- `mps2_an505.ld`: FLASH 0x10000000(4MB, vector先頭), RAM 0x38000000(4MB)。SafeG-M の ld 形式
  （`mimxrt685.ld` 構造：`.vector`先頭, Global Section Table, istack 等）に合わせて再構成。
- `cmsdk_uart.c`/`target_serial.c`: UART0=0x40200000。非TECS SIO（方式S1）。
- `target_kernel_impl.c`: `hardware_init_hook` の **FPU有効化(CPACR)+DSB/ISB** は QEMU で必須
  （ISB無しだと FP命令が NOCP UsageFault になる）＝**そのまま移植**。`target_initialize` は
  `core_initialize()`＋SIO init に、**SafeG-M用に SAU/ITNS 設定を追加**。
- `target_timer.c`(任意): SYSTICK を使うなら common/core_timer で足りる。ASP3CORE版は HRT 実装が別。

## 4. SAU / メモリ配置（SafeG-M有効時）
an505 アドレス空間で Secure/NS を分割（SSE-200 メモリマップ）:
- Secure: コード 0x10000000、RAM 0x38000000（現 ld）。
- NS: 非エイリアス側 0x00000000 / 0x28000000 を割当（要 SAU NS リージョン定義）。`TOPPERS_NS_VTOR` を
  NS ベクタ先頭に設定（mimxrt685 は 0x8400000）。現 mimxrt685 の SAU 値（`target_kernel_impl.c` の
  `target_initialize`, NSC/NS-Flash/NS-RAM の3リージョン）を an505 アドレスへ置換。
- NSC(Secure gateベニア)領域も an505 内に確保（mimxrt685 は 0x183FFE00）。

## 5. 必要な新規ファイル（`asp3/target/mps2_an505_gcc/`）
- `Makefile.target`（本コミットで scaffold 追加：BOARD=mps2_an505, PRC=arm_m, TOOL=gcc, CHIP=<薄chip>,
  KERNEL_TIMER=SYSTICK, start.o, ld 選択, SAFEG on/off）。
- `mps2_an505.ld`（Secure用。本コミットで scaffold 追加）／ SafeG有効時の NS 分割版 `mps2_an505_s.ld`。
- `target_kernel_impl.{c,h}`（hardware_init_hook=FPU+ISB、target_initialize=core_initialize+SAU、target_exit=SYS_EXIT）。
- `target_kernel.cfg`/`target_kernel.trb`/`target_check.trb`/`target_cfg1_out.h`。
- `target_{kernel,sil,stddef,syssvc,test,rename,unrename}.h` ＋ `target_rename.def`。
- SIO: 方式S1なら `cmsdk_uart.c`+`target_serial.{c,cfg,h}`(非TECS)／方式S2なら `tSIOPortTarget*` TECS。
- 薄い chip 層 `asp3/arch/arm_m_gcc/<chip>/Makefile.chip`（`CORE_TYPE=CORTEX_M33`＋`include Makefile.core`）
  と chip 各ヘッダ（imxrt600 の `chip_*.h` 計~685行を雛形に、SSE-200/CMSDK 用へ簡略化）。
  ※ chip を新設せず common 直結にするには Makefile.target の `include .../$(CHIP)/Makefile.chip` を
  汎用 chip に向ける必要あり。最小 chip 新設が無難。

## 6. ビルド & QEMU 実行（目標コマンド）
```sh
# Secure (まず 小: ENABLE_SAFEG_M=0)
cd asp3 && mkdir build_an505 && cd build_an505
ruby ../configure.rb -T mps2_an505_gcc            # 方式S1なら -w を追加(TECS不使用)
make ENABLE_SAFEG_M=0
# QEMU 起動（必ず timeout で囲む）
timeout 20 qemu-system-arm -M mps2-an505 -nographic \
  -semihosting-config enable=on,target=native -kernel asp   # asp=ELF

# 中: SafeG-M有効 + NS 2イメージ
make ENABLE_SAFEG_M=1                              # Secure
# NS(ベアメタル test) を an505 NS アドレスへリンク → ns.bin/elf
timeout 30 qemu-system-arm -M mps2-an505 -nographic \
  -semihosting-config enable=on,target=native \
  -kernel asp -device loader,file=ns.elf           # 2イメージ(TF-M流)
```
- 出力は CMSDK UART0(0x40200000) → `-nographic`/`-serial mon:stdio`。RAM は **0x38000000**（0x30000000はバッキング無）。
- CI判定: `[TST] DONE` & `fail=0` を UART ログから grep、または SYS_EXIT 終了コードで判定するスクリプト化。
- 機序確認時は `-S -gdb tcp::1234` ＋ gdb batch(+timeout) で CFSR/UFSR/PSPLIM_S を読む。

## 7. リスク / 実機との差
- **`deactivate_nonsecure_interrupts`(UsageFault状態機械, `core_support.S:622`)**: 最も実機依存的。
  NS割込みアクティブ経路の QEMU 再現は要検証（NS割込み無し経路はまず通る）。
- **FP/STKOF**: QEMU は STKOF 実装済・FP遅延スタッキング忠実。修正(A)前の HardFault 再現や、
  STKOF(bit20)/NOCP(bit19)/SecureFault の読み分けに QEMU が有用（実機 gdbserver 不安定の代替）。
- ハング: **全 qemu/gdb 実行を `timeout` で囲む**（セミホスティング待ちでハングするため）。
- 既存 mimxrt685 ターゲット・test・asp3本体(修正(A)以外)は不変のまま、an505 は新規追加で並存。

## 8. 推奨ステップ
1. 方式S1の前提（非TECS syssvc の有無）を確認 → SIO 方式確定。
2. 薄chip + Makefile.target + ld + 非TECS SIO + target_kernel_impl(FPU/ISB) で **小: Secure単独起動**
   （ENABLE_SAFEG_M=0、syslog バナーが QEMU UART に出ることを確認）。
3. SAU/ITNS/launch_ns を an505 アドレスで有効化、NS をリンクし **中: 2イメージ起動 + gate往復**。
4. `test/` を an505 で実行、A〜D1 の `[TST]` を確認、SYS_EXIT でCIスクリプト化。
5. deactivate 経路を QEMU で詰める（実機と比較）。

## 9. 本コミットの scaffold（追加のみ・未ビルド検証）
- `asp3/target/mps2_an505_gcc/Makefile.target`（WIP scaffold）
- `asp3/target/mps2_an505_gcc/mps2_an505.ld`（WIP scaffold, Secure/非SafeG用）
これらは出発点。`target_kernel_impl.c`/SIO/chip/cfg-trb は未作成（§5参照）。本パスでは**ビルド成功までは未到達**。

---

## 9. 実装記録（小フェーズ: Secure単独起動の実機確認）

**到達: 「小」達成 — ASP3(Secure単独, ENABLE_SAFEG_M=0)が QEMU mps2-an505 でビルド・起動し，
CMSDK UART に syslog バナーを出力することを確認．**

### 作成・変更ファイル
- chip層 `asp3/arch/arm_m_gcc/mps2_an505/`（nrf5340 chip を複製して改変）
  - `tUsart.c`（新規, CMSDK APB UART の TECS SIO 実装）
  - `mps2_an505.h`（ASP3CORE から流用, レジスタ/IRQ/クロック定義）
  - `chip_sil.h`（`#include "NRF5340.h"` → `mps2_an505.h`）
  - その他 `Makefile.chip`/`chip_*.h`/`tUsart.cdl` は nrf5340 のまま流用
- target層 `asp3/target/mps2_an505_gcc/`
  - `Makefile.target`（scaffold: CHIP=mps2_an505, KERNEL_TIMER=SYSTICK, start.o）
  - `mps2_an505.ld`（FLASH 0x10000000 / RAM 0x38000000, 共通 start.S 用シンボル）
  - `target_kernel_impl.c`（新規: hardware_init_hook=CPACRでFPU有効化+DSB/ISB,
    target_initialize=core_initialize+tPutLogSIOPort_initialize, target_exit=半ホスティングSYS_EXIT）
  - `target_timer.h`（新規: SysTick用に core_timer.h を include）
  - `mps2_an505_dk.h`（新規: SIOPORT1_BASE=UART0 0x40200000, SIOPORT1_IRQ=combined 42）
  - `tSIOPortTarget.cdl`（import を an505 ヘッダへ, SIOPortTarget1 のみに整理）
  - `target.cdl`（BannerTargetName=ARM MPS2-AN505:Cortex-M33）, `target_syssvc.h`/`target_stddef.h`（名称）
  - `target_kernel.cfg`（`INCLUDE("core_timer.cfg")` ＝ SysTick）
  - 他ヘッダ/trb は nrf5340 から流用

### ビルド & 実行
```
cd asp3 && mkdir build_an505 && cd build_an505
ruby ../configure.rb -T mps2_an505_gcc        # exit 0
make                                          # exit 0 (FLASH 22.9KB / RAM 15.5KB)
timeout 15 qemu-system-arm -M mps2-an505 -cpu cortex-m33 -nographic \
  -semihosting-config enable=on,target=native -kernel asp
```
出力（要点）:
```
TOPPERS/ASP3 Kernel Release 3.7.0 for ARM MPS2-AN505:Cortex-M33
System logging task is started.
Sample program starts (exinf = 0).
task1 is running (001).   |
```
→ **バナー・ログタスク・サンプル開始・task1 実行を UART で確認＝「小」成功条件を満たす．**

### 既知の不具合（次フェーズで要修正）
1. **`no time event is processed in hrt interrupt.` が毎tick出力**（time_event.c:631, LOG_NOTICE）．
   SysTick(1ms tick, TSTEP_HRTCNT=1000)で signal_time を周期呼出しする一方，target_hrt_set_event が
   空（イベント駆動でない）ため，イベント無しtickで通知が出る．動作は継続するがノイズ．
2. **task1 が001を出した直後に CPU 例外で停止**（`Sample program ends with exception`）．
   - gdb 解析: **CFSR=0x00040000 = UFSR.INVPC（例外リターン時の不正PCロード）**，
     スタックされた PC = `_kernel_call_alarm`（alarm.c:226）．
   - **FPU 無効ビルドでも同一再現＝FP退避とは無関係**．
   - 推定原因: SafeG-M には **ENABLE_SAFEG_M=0（非TrustZone）の M33 構成の前例が無く**（mimxrt685/nrf5340 は
     既定で SAFEG=1），非TZ M33 の例外リターン経路 or SysTick 時間イベント処理（alarm/cyclic コールバック）の
     未検証パスを踏んでいる可能性が高い．

### 次ステップ（中フェーズ）
- (推奨) **ENABLE_SAFEG_M=1（TrustZone有効＝既存の検証済み構成）で再挑戦**し，#2 の非TZ未検証経路を回避．
  併せて AN505 アドレス空間の SAU/ITNS 設定（Secure 0x10000000/0x38000000, NS 0x00000000/0x28000000,
  NSC 領域）を target_initialize に実装，NS(FreeRTOS/ベアメタル)を `-kernel`+`-device loader` で2イメージ起動．
- もしくは #2 を非TZ単独で根治（core_support.S の非TZ M33 EXC_RETURN / time-event コールバック経路の精査）．
- #1 は SysTick HRT 統合（target_hrt_set_event のイベント駆動化 or 通知抑制）で解消．

---

## §10 実装記録（中フェーズ・SAFEG=1 2イメージ起動）

### 到達点（確定）
- **SAFEG=1 で Secure テストアプリ + NS ベアメタルの 2イメージが QEMU mps2-an505 で起動し，
  AI ハーネス [TST] 出力が UART に出る**ところまで到達。
  - Secure: FLASH 15.6KB / SG_veneer 96B(=gate veneer 生成) / RAM 30.9KB。CMSE implib
    `FreeRTOS/sample/mps2_an505_gcc/secure_nsclib.o` 生成 → NS がリンク。
  - `[TST] RESULT_ADDR 0x38000020`（an505 Secure SRAM）も正しく出力。
- CI スクリプト `tools/ci_an505.sh` 追加（2イメージ起動→[TST] DONE & fail=0 で pass/fail 判定，
  exit 0/1）。現状は下記ブロッカーにより FAIL を返す（ハーネス自体は正常動作）。

### 追加/変更ファイル（asp3 共通本体は未改変）
- `asp3/target/mps2_an505_gcc/mps2_an505_s.ld`（新規: Secure/NS 分割。Secure 0x10000000(2MB),
  NSC veneer 0x101FFE00(.gnu.sgstubs), RAM 0x38000000(2MB)）
- `asp3/target/mps2_an505_gcc/target_kernel_impl.c`（SAU 3領域を実装: NSC 0x101FFE00 /
  NS code 0x00200000 / NS RAM 0x28200000，SAU_CTRL_ENABLE）
- `asp3/target/mps2_an505_gcc/Makefile.target`（TOPPERS_NS_VTOR=0x00200000）
- `test/ns_baremetal/mps2_an505_gcc/`（新規: ns_an505.ld[NS 0x00200000/0x28200000],
  startup_ns.c[最小ベクタ+ResetISR], Makefile[implib をリンク]）
- `tools/ci_an505.sh`（新規）

### QEMU 起動コマンド（2イメージ）
    qemu-system-arm -M mps2-an505 -cpu cortex-m33 -nographic \
      -semihosting-config enable=on,target=native \
      -kernel asp3/build_an505_test/asp \
      -device loader,file=test/ns_baremetal/mps2_an505_gcc/nstest.bin,addr=0x00200000

### ブロッカー（要対応・次ステップ）
- **Secure 側の強制 HardFault**（test_start 直後）。Secure hardfault_handler が捕捉し
  `CP D1 209` / `CHK 209 exp=1 act=0`(=from_ns=0, EXC_RETURN.S=1) / `MARK 0xF004`(=HFSR.FORCED)
  を出力。**NS/TrustZone 起因ではなく Secure 内の configurable fault エスカレーション**。
  小フェーズで観測した **UFSR.INVPC（例外リターン時の不正PC, stacked PC=_kernel_call_alarm）**
  と同一系統と判断（FPU 無効でも再現＝FP退避とは無関係）。
- 併発: `no time event is processed in hrt interrupt.` の毎tick出力。SafeG-M 共通 core_timer.c は
  **周期 timetick 型**（`hrtcnt_current += TSTEP_HRTCNT`，TSTEP=1000us, CPU_CLOCK_HZ=20MHz で reload 妥当）
  だが，カーネルの HRT モデルとの整合で NOTICE が毎回出る。動作は継続するノイズ。
- **根本原因の所在**: an505 の SysTick/HRT タイマドライバ統合（時間イベント/alarm 経路の
  例外リターン整合）。TrustZone/SAU/NS 配置・gate・implib は機能している。
- 次ステップ: (1) core_timer の HRT モデルをカーネル期待（イベント駆動 set_event）に合わせる
  か周期 timetick で signal_time 経路を正す，(2) 例外リターン INVPC を gdb（mps2-an505 は
  CFSR/UFSR/PSPLIM_S 読取り可）で特定，(3) 解消後 ci_an505.sh で A/B/C/D1 を green 化。

---

## §13 #13 再調査の確定結果（2026-06-16, gdb/-d int で機序確定）

ロックアップ（`Lockup: can't escalate 3 to HardFault, priority -1`）は **解消済み**（MPC 設定＋
正しいビルドにより NS は起動し，フォールトは HardFault へ正常エスカレートして
`hardfault_handler` が捕捉する）。残る唯一のブロッカーは下記。

### 確定した根本原因: SG ゲートウェイでの SecureFault INVEP（QEMU 固有）
- `-d int` ログ: `Prefetch Abort ... at fault address 0x101ffe20 ... really SecureFault with
  SFSR.INVEP`。**最初の NS→Secure gate 呼出し**（`tg_get_phase` の SG veneer = 0x101ffe20）で
  INVEP（無効エントリポイント）が発生し，A/B/C へ進めない。`hardfault_handler` が
  これを `from_ns=1` で捕捉し **D1 のみを記録**して DONE（＝従来 ci が PASS と誤判定していた所以）。
- 直前は Secure SysTick(exc15) リターン（EXC_RETURN magic=0xFFFFFFFD＝**Secure 背景**）で，
  SG 命令(0x101ffe20)再フェッチ時に INVEP。割込み嵐ではない（12 秒で SysTick 1251 回・
  **INVEP は 1 回のみ**の決定論的フォールト）。

### 検証で「正常」と確定した項目（gdb 実機読み戻し）
- SAU: 8 リージョン，CTRL=1。**R0 = NSC @0x101FFE00（RBAR=0x101ffe00, RLAR=0x101fffe3＝
  NSC+ENABLE）**，R1=NS code 0x00200000，R2=NS RAM 0x28200000 — すべて意図通り
  （以前「全リージョンが R2 に見えた」のは gdb が SAU_RNR を切替えられない読み出し限界の
  アーティファクトで，CPU 側読み戻しで否定）。
- SG veneer: `0x101ffe20: sg; b.w __acle_se_tg_get_phase` が NSC 領域に正しく配置。
- MPC（0x58007000/0x58009000）・NS エイリアス・implib（secure_nsclib.o, ビルド順）すべて正常。

### 結論
- 同一現象が **QEMU 8.2.2 と 11.0.1 の両方で再現**（バージョン依存ではない）。
- **実機 mimxrt685 では A〜D1 が 9/9 PASS**（NSC 配置 0x183FFE00, NS 0x08400000）。
- → これは SafeG-M 側のバグではなく，**QEMU mps2-an505 の SG ゲートウェイ＋Secure例外
  相互作用の挙動差（エミュレーションのコーナーケース）**と判断する。SAU/SG/NSC/MPC は
  すべて実機読み戻しで正常を確認済み。
- したがって **QEMU での A〜D1 green は本経路では達成不可**。実機が遷移テストの正とし，
  QEMU はビルド/非遷移系の CI に用いる位置づけが妥当。

### 本調査での変更（クリーン）
- `tools/ci_an505.sh`: `CP A1`/`CP B1` の通過を必須化し，「D1 のみ」の退行を **PASS と
  誤判定しない**よう修正（現状は正しく FAIL を返す）。
- `target_kernel_impl.c` に入れた一時診断（SAU 読み戻し）は撤去済み（ツリーはクリーン，
  ビルド config check passed）。
- 未解決として #13 は「QEMU 限界」を根拠に保留継続（実機検証で代替）。深掘りするなら
  QEMU 側 `armsse` IDAU/SG 経路のソースデバッグが必要。
