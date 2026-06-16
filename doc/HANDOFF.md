# SafeG-M リファクタリング 引き継ぎ（別PC再開用）

最終更新: 2026-06-17。このファイルは**別PCで作業再開するための最重要メモ**。両リポジトリと一緒に git で運ぶこと。

## 0. 一言サマリ
SafeG-M(ARMv8-M TrustZone Dual-OS: Secure=ASP3 / NS=FreeRTOS等) を、**外部の素 asp3_core(CMake/非TECS/3.7.2) に SafeG 改変を `#ifdef TOPPERS_SAFEG_M` で取り込む「案1」**へ載せ替え中（M0で凍結）。**実機 i.MX RT685 でフル遷移テスト A〜D1 = 9/9 PASS を達成済み**（`doc/asp3core_imxrt685_full_green.md`）。

## 1. リポジトリと配置（重要）
- **SafeG-M**: `git@github.com:exshonda/SafeG-M`（このリポジトリ）。board/sample/test/FreeRTOS/tools/doc を保持。
- **asp3_core**: `git@github.com:exshonda/asp3_core`（別リポジトリ・ユーザー管理）。Secure側カーネル本体＋SafeG TZ取込み先。
- **配置（workspace sibling）**: 2リポジトリを横並びに clone し、SafeG-M のビルドは `-DASP3CORE_DIR=<asp3_coreのパス>` で参照（submodule不採用、M0 §9/Q3）。別PCでは両方 clone すること。
- 旧構成（asp3_3.7 を丸ごと同梱＋safeg.patch、overlay版）は `main`〜`m3-nontecs` 系列に履歴あり。**現行の正は案1（asp3_core）**。

## 2. ブランチ（統合済み・別PChでcheckout対象）
- **SafeG-M `m3-nontecs`**: test非TECS化＋[TST]同期出力＋an505/RP2350/imxrt685 NS＋設計doc。imxrt685の9/9記録(`8801ed1`)を ff 統合済。
- **asp3_core `safeg-m-m1`**: SafeG TZ載せ替え本体（M1〜M4）＋an505/pico2/mimxrt685 SAFEGボード層。imxrt685ボード層(`3b548ab`)を ff 統合済。
- どちらも **main へは未マージ**（必要なら別PCで PR/merge）。push 済（origin の同名ブランチ）。

## 3. 達成状況（M0案1）
- M0 設計凍結（`M0_design_freeze.md` がワークスペース直下＝repo外。`doc/M0_design_freeze.md` に取込み済）。
- M1: asp3_core 素ビルド＋SAFEG=0起動、core_support.S pendsv/svc(#else swap)。
- M2: launch_ns/SAU/ITNS/IIPM_ENAALL/arm_m.h純追加/chip → SAFEG=1 機能成立（QEMU an505でSecure起動）。
- M3-enable/M3: `_SAFEG_BTASK`(id=1) cfg生成、CMSE implib(opt-in `ENABLE_SAFEG_IMPLIB`)、test非TECS化(`test_safeg.cdl`廃止/`.cfg`書換え)、[TST]同期出力化。
- M4: RP2350/imxrt685 SAFEGボード層。**実機RP2350でNS launch完走・INVEP無し**、**実機imxrt685で A〜D1 9/9 green**。
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
- **RP2350(Pico2) SWD-lock**: SAFEG=1ファーム実走後、SWDデバッグがロック（セキュアデバッグ挙動）→ SWD反復書込み困難。**実機フル取得は BOOTSEL+UF2/picotool（USBマスストレージ・SWD非依存）経路が必要**（未実施＝残課題、`doc/rp2350_bringup.md`）。遷移自体はRP2350実機でNS launch完走・INVEP無しを確認済。
- **[TST]出力**: 元は syslog(LOG_EMERG)で非同期(LogTaskドレイン)＋二重出力。`test_start()`でlogmaskからLOG_EMERG除外→lowmask即時(`target_fput_log`)のみ＝LogTask非依存・1回・同期(`test/common/test_harness.c`)。
- **CMSE implib**: gate無アプリ(sample)では `--out-implib` が "no symbols" 失敗→`ENABLE_SAFEG_IMPLIB`(既定OFF)でgate保有アプリ時のみ最終ELFに付与。
- **asp3_core構造**: CMake＋非TECS＋`core_*.py`。SafeG所有の `core_kernel.trb`/`Makefile.core`/`tecsgen` は無い。M系chipは imxrt600/rp2350、target は an505/pico2_arm/mimxrt685evk が SAFEG対応済。CMSIS衝突C1=bare `CPACR`/`FPCCR`非持込、`CPACR_BASE`/`FPCCR_ADDR`/`FPCCR_NS_ADDR`使用。

## 6. ハードウェア環境（このPC固有・別PCでは要再確認）
- imxrt685 EVK: LPC-LINK2 `ISA0BQNQ`、VCOM `/dev/ttyACM1` @115200、LinkServer `/usr/local/LinkServer_26.5.59/LinkServer`。**`load`後 boot-ROM stall で自走しないことあり→RESET/電源再投入で復旧**。
- RP2350(Pico2): Pico debugprobe `2e8a:000c`、openocd `/usr/local/bin/openocd`(RPiフォーク, RP2350対応)、`interface/cmsis-dap.cfg`+`target/rp2350.cfg`+`transport select swd; adapter speed 5000`、VCOM `/dev/ttyACM2`。**劣化時は BOOTSEL(押しながら電源)で復旧**。
- QEMU: `qemu-system-arm 8.2.2`(及び `/home/honda/qemu-build` の 11.0.1)、`-M mps2-an505 -cpu cortex-m33`。

## 7. 残課題（別PCで継続）
1. **RP2350 実機フル A〜D1**：UF2/picotool 経路（SWD-lock回避、要Pico2自身のUSB接続＋picotool）。`doc/rp2350_bringup.md`。
2. ブランチを `main` へマージ/PR、版固定（使用 asp3_core commit を記録）。
3. tecsgen の external化（当面SafeG所有）、CLAUDE.md 最終反映。
4. 出力経路の実機差（RP2350のlowmask即時がPL011に届くか）の最終確認（imxrt685はLogTask/同期とも実績）。

## 8. 主要ドキュメント索引
- `doc/M0_design_freeze.md` … 案1の設計凍結（差分4分類、core_support.S 9サイト、EXC_RETURN matrix 等）
- `doc/asp3core_migration.md` … 案1 vs 案2比較（SUPERSEDED, M0が正）
- `doc/asp3core_imxrt685_full_green.md` … 実機9/9 green記録・再現手順
- `doc/m2_loadin_spec.md` / `doc/m3_nontecs_test.md` / `doc/rp2350_bringup.md` … 各フェーズ設計
- `doc/analysis_imxrt685.md` … SafeG-M(旧)ソース解析（launch_ns/deactivate/SAU を file:line で）
- `doc/test_safeg.md` … 遷移テスト([TST])仕様・CP表
- `doc/step2_asp3_3.7.2_update.md` / `doc/step3_overlay.md` / `doc/step4_overlay.md` … 旧overlay系列(asp3_3.7)の記録
