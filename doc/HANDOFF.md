# SafeG-M リファクタリング 引き継ぎ（別PC再開用）

最終更新: 2026-06-17。このファイルは**別PCで作業再開するための最重要メモ**。両リポジトリと一緒に git で運ぶこと。

## 0. 一言サマリ
SafeG-M(ARMv8-M TrustZone Dual-OS: Secure=ASP3 / NS=FreeRTOS等) を、**外部の素 asp3_core(CMake/非TECS/3.7.2) に SafeG 改変を `#ifdef TOPPERS_SAFEG_M` で取り込む「案1」**へ載せ替え中（M0で凍結）。**実機 i.MX RT685 でフル遷移テスト A〜D1 = 9/9 PASS を達成済み**（`doc/asp3core_imxrt685_full_green.md`）。**さらに 2026-06-17、主ターゲットである実機 RP2350(Pico2) でも SWD 経路でフル A〜D1 = 9/9 PASS を達成**（`doc/rp2350_bringup.md` 末尾。M0確定の「遷移の正=RP2350」を本実機 green で充足）。

## 1. リポジトリと配置（重要）
- **SafeG-M**: `git@github.com:exshonda/SafeG-M`（このリポジトリ）。**`doc / FreeRTOS / test / tools` のみ保持（分類D 極小化）**。2026-06-17 に旧 overlay ツリー `asp3/`(同梱asp3_3.7+tecsgen) と旧 TZ ポート `target/`(lpc55s69/mimxrt685/nrf5340/stm32l552) 計858ファイルを削除（案1 の現役パスは元々 TECS フリーで参照ゼロ。旧物は git 履歴に残存）。
- **asp3_core**: `git@github.com:exshonda/asp3_core`（別リポジトリ・ユーザー管理）。Secure側カーネル本体＋SafeG TZ取込み先。
- **配置（workspace sibling）**: 2リポジトリを横並びに clone し、SafeG-M のビルドは `-DASP3CORE_DIR=<asp3_coreのパス>` で参照（submodule不採用、M0 §9/Q3）。別PCでは両方 clone すること。
- 旧構成（asp3_3.7 を丸ごと同梱＋safeg.patch、overlay版）は `main`〜`m3-nontecs` 系列に履歴あり。**現行の正は案1（asp3_core）**。

## 2. ブランチ（**両リポジトリとも main に統合済・push 済**）
- **SafeG-M**: `main`(=旧 `m3-nontecs`, `27c1a15`) に統合済。test非TECS化＋[TST]同期出力＋an505/RP2350/imxrt685 NS＋設計doc＋imxrt685の9/9記録(`8801ed1`)。`origin/main` と一致。
- **asp3_core**: `main`(=旧 `safeg-m-m1`, `3b548ab`) に統合済。SafeG TZ載せ替え本体（M1〜M4）＋an505/pico2/mimxrt685 SAFEGボード層。`origin/main` と一致。
- **2026-06-17 に両ブランチを `main` へ ff マージし push 済**（旧記述「main未マージ」は解消。残課題 §7-2 のマージ作業は完了）。別PCでは `main` をそのまま使えばよい。

## 3. 達成状況（M0案1）
- M0 設計凍結（`M0_design_freeze.md` がワークスペース直下＝repo外。`doc/M0_design_freeze.md` に取込み済）。
- M1: asp3_core 素ビルド＋SAFEG=0起動、core_support.S pendsv/svc(#else swap)。
- M2: launch_ns/SAU/ITNS/IIPM_ENAALL/arm_m.h純追加/chip → SAFEG=1 機能成立（QEMU an505でSecure起動）。
- M3-enable/M3: `_SAFEG_BTASK`(id=1) cfg生成、CMSE implib(opt-in `ENABLE_SAFEG_IMPLIB`)、test非TECS化(`test_safeg.cdl`廃止/`.cfg`書換え)、[TST]同期出力化。
- M4: RP2350/imxrt685 SAFEGボード層。**実機RP2350で A〜D1 フル 9/9 green**(2026-06-17, SWD経路, `doc/rp2350_bringup.md`)、**実機imxrt685で A〜D1 9/9 green**。
- M4+: **実機RP2350 で NS=FreeRTOS も成立**(2026-06-17, `test/ns_freertos/pico2_arm_gcc` + `FreeRTOS/sample/pico2_arm_gcc`, ARM_CM33_NTZ port, A/B `SUMMARY total=5 pass=5`)。NS はベアメタル＋FreeRTOS の2種が RP2350 実機で確認済。
- 全工程 **SAFEG=0 非回帰維持**（変更は `#ifdef TOPPERS_SAFEG_M` ガード、既定OFFで素ASP3不変）。

## 4. 実機フル A〜D1 の再現手順（i.MX RT685、確定済み）
詳細・トランスクリプトは `doc/asp3core_imxrt685_full_green.md`。要点:
1. **Secure ビルド**(asp3_core CMake): `--preset <pico2_arm|mimxrt685evk相当>` ＋ `-DENABLE_SAFEG_M=ON -DENABLE_SAFEG_IMPLIB=ON -DASP3_APPLNAME=test_safeg -DASP3_APPLDIR=<SafeG-M/test/secure> -DASP3_EXTRA_APP_C_FILES="<test/secure/test_gate.c>;<test/common/test_harness.c>" -DASP3_APP_INCLUDE_DIRS=<test/common>` → `asp.srec` ＋ `secure_nsclib.o`(gate implib)。
2. **NS ビルド**(`SafeG-M/test/ns_baremetal/mimxrt685evk_gcc`): `make EXTRA_CFLAGS="-DTST_ENABLE_A3 -DTST_ENABLE_C -DTST_ENABLE_D1"`（**TST_ENABLE_* は NS側のみ有効**＝Secureは常時全ハンドラ）。`secure_nsclib.o` をリンク。NS_VTOR=0x8400000。
3. **書込み**(LinkServer, probe `ISA0BQNQ`, device `MIMXRT685S:EVK-MIMXRT685`): Secure `load asp.srec`、NS `load nstest.axf`。
4. **取得**: **デバッガ非接続で RESET 自走が必須**（LinkServer run 等デバッガ常駐だと D1 の NS udf→HardFault を handler前にhaltしてD1が出ない）。**`cat /dev/ttyACM1`(115200) を先に開始してから RESET ボタン**（[TST] はワンショット）。→ `SUMMARY total=9 pass=9 fail=0 / DONE`。

## 5. 重要な発見・ハマりどころ（記憶）
- **FPU退避バグ 修正(A)**: gate veneer の FPスクラッチで BTASK 文脈に `CONTROL_S.FPCA=1` が残り、BTASK横取りディスパッチ時の `vstmdb {s16-s31}` が HardFault。`core_support.S` で **BTASKは FP退避/復帰をスキップ**して解消。ASP3更新時の最重要回帰項目。C カテゴリ実行で非回帰確認。
- **QEMU(mps2-an505) の INVEP 限界**: 最初の Secure gate で SecureFault `SFSR.INVEP`（QEMU 8.2.2/11.0.1 で再現、実機では出ない）＝**QEMUのSG/Secure例外エミュ限界**。→ QEMUはビルド/非遷移CI、**遷移テストの正は実機**。
- **RP2350(Pico2) SWD 経路で全 green（2026-06-17 達成）**: 過去に「SAFEG=1ファーム実走後 SWDロック→reset/halt timeout」の劣化を観測し UF2/picotool 経路を想定していたが、**クリーン状態(`reset halt` が通る)なら SWD で書込み→フル A〜D1 9/9 取得できることを実証**。**鍵は openocd を `program ... verify` 後に `reset; exit` で切断し自走させること**（デバッガ常駐だと D1 の NS udf→HardFault を handler 前に halt して D1 が出ない＝imxrt685 と同じ罠）。劣化(reset/halt timeout)が出たら BOOTSEL(押しながら電源)で物理復旧してから再試行、それでも駄目なら UF2/picotool が保険。
- **[TST]出力**: 元は syslog(LOG_EMERG)で非同期(LogTaskドレイン)＋二重出力。`test_start()`でlogmaskからLOG_EMERG除外→lowmask即時(`target_fput_log`)のみ＝LogTask非依存・1回・同期(`test/common/test_harness.c`)。
- **CMSE implib**: gate無アプリ(sample)では `--out-implib` が "no symbols" 失敗→`ENABLE_SAFEG_IMPLIB`(既定OFF)でgate保有アプリ時のみ最終ELFに付与。
- **asp3_core構造**: CMake＋非TECS＋`core_*.py`。SafeG所有の `core_kernel.trb`/`Makefile.core`/`tecsgen` は無い。M系chipは imxrt600/rp2350、target は an505/pico2_arm/mimxrt685evk が SAFEG対応済。CMSIS衝突C1=bare `CPACR`/`FPCCR`非持込、`CPACR_BASE`/`FPCCR_ADDR`/`FPCCR_NS_ADDR`使用。

## 6. ハードウェア環境（このPC固有・別PCでは要再確認）
- imxrt685 EVK: LPC-LINK2 `ISA0BQNQ`、VCOM `/dev/ttyACM1` @115200、LinkServer `/usr/local/LinkServer_26.5.59/LinkServer`。**`load`後 boot-ROM stall で自走しないことあり→RESET/電源再投入で復旧**。
- RP2350(Pico2): Pico debugprobe `2e8a:000c`、openocd `/usr/local/bin/openocd`(RPiフォーク, RP2350対応)、`interface/cmsis-dap.cfg`+`target/rp2350.cfg`+`transport select swd; adapter speed 5000`。**劣化時は BOOTSEL(押しながら電源)で復旧**。
  - **このマシン(2026-06-17 作業機)固有**: VCOM=**`/dev/ttyACM0`**(Debugprobe VCP, 旧記録の ttyACM2 から変化＝接続後 `udevadm info` で要再確認)。Debugprobe FW=2.0.1(古い→openocd が low-performance workaround を表示するが書込みは成功)。**ビルドツールは別系統**: cmake/ninja は `apt`(cmake 3.28.3/ninja 1.11.1)、arm toolchain は `/usr/local/tools/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi/bin`（旧記録の `~/.mcuxpressotools` 同梱 cmake/ninja は無い）。asp3_core は `/home/honda/TOPPERS/safeg-m/asp3_core`、SafeG-M は `/home/honda/TOPPERS/safeg-m/SafeG-M`。
- QEMU: `qemu-system-arm 8.2.2`(及び `/home/honda/qemu-build` の 11.0.1)、`-M mps2-an505 -cpu cortex-m33`。

## 7. 残課題（別PCで継続）
1. ~~**RP2350 実機フル A〜D1**~~：**完了（2026-06-17, SWD経路で 9/9 green、`doc/rp2350_bringup.md` 末尾）**。UF2/picotool 経路は SWD 劣化時の保険として未実施のまま残置（必須ではない）。
2. ~~ブランチを `main` へマージ/PR~~：**完了（両 repo とも main 統合・push 済、§2）**。残るは版固定＝SafeG-M と asp3_core の使用 commit 対応の記録（現状: SafeG-M `27c1a15` × asp3_core `3b548ab`）。
3. ~~tecsgen の external化／CLAUDE.md 最終反映~~：**完了（2026-06-17）**。SafeG-M から `asp3/`＋tecsgen＋旧 `target/` を削除（極小化）。`AGENTS.md`(正本)＋薄い `CLAUDE.md` を新規作成（案1ビルド手順・禁則・版固定 `SafeG-M 27c1a15 × asp3_core 3b548ab`）。
4. 出力経路の実機差（RP2350のlowmask即時がPL011に届くか）の最終確認 → **RP2350 実機で [TST] 同期出力が `/dev/ttyACM0` に全行届くことを確認済（PL011/lowmask 即時 OK）**。imxrt685 はLogTask/同期とも実績。
5. **コードレビュー由来の項目（2026-06-17 のレビュー）**。即修正5点は適用済、残は #3 のみ（要判断）:
   - **修正済（実機 9/9 再確認済）**: #1 CMSE gate の NS ポインタ検証（`test/secure/test_gate.c`）／#2 `set_control_ns` ISB／#5 `TOPPERS_NS_VTOR` コンパイル時 assert（asp3_core, `722c445`）／#8 `test_harness.c` の `buf` ローカル化（例外文脈再入対策）／doc CHK3/4 注記修正。
   - **#6 S→NS FP scrub = 対応不要（解決）**: `cmse_nonsecure_call` の生成 veneer `__gnu_cmse_nonsecure_call` が **`vlstm`/`vlldm`** で Secure FP(S0-S31/FPSCR) を退避・クリアする＝アーキ標準の FP scrub 済（`/tmp/run_sec2/asp.elf` 逆アセンブルで確認）。レビュー指摘は C/asm ソースのみで veneer 未確認だったため。残懸念は Cortex-M33 VLLDM erratum が RP2350 r1p0 に該当するかの確認のみ（必要時）。
   - **#3 RP2350 ACCESSCTRL（保留・要判断）**: 当初 High と評価したが再精査の結果**限界利益は小**。現 SAU 設定が NS 領域(R0 NSC/R1 code/R2 RAM)以外を全て Secure 属性とし、NS→Secure SRAM/ペリフェラルは **SAU/IDAU が既に SecureFault で遮断**済。ACCESSCTRL はバス層の belt-and-suspenders で、`RP2350.h` に定義が無く 0x40060000 系への生書込み追加＝実機破壊リスクがある。**現脆弱性ではない**ため、やるなら datasheet 精査＋実機再検証を伴う独立タスクとして実施判断する（`asp3_core/target/pico2_arm_gcc/target_kernel_impl.c` のコメント参照）。

## 8. 主要ドキュメント索引
- `doc/M0_design_freeze.md` … 案1の設計凍結（差分4分類、core_support.S 9サイト、EXC_RETURN matrix 等）
- `doc/asp3core_migration.md` … 案1 vs 案2比較（SUPERSEDED, M0が正）
- `doc/asp3core_imxrt685_full_green.md` … 実機9/9 green記録・再現手順
- `doc/m2_loadin_spec.md` / `doc/m3_nontecs_test.md` / `doc/rp2350_bringup.md` … 各フェーズ設計
- `doc/analysis_imxrt685.md` … SafeG-M(旧)ソース解析（launch_ns/deactivate/SAU を file:line で）
- `doc/test_safeg.md` … 遷移テスト([TST])仕様・CP表
- `doc/step2_asp3_3.7.2_update.md` / `doc/step3_overlay.md` / `doc/step4_overlay.md` … 旧overlay系列(asp3_3.7)の記録
