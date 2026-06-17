# AGENTS.md

> **SafeG-M**（リポジトリ：`SafeG-M`）における、全AIコーディングツール共通の正本。
> Claude Code / Cline / Cursor 等のツール固有ファイル（`CLAUDE.md` / `.clinerules` / `.cursorrules`）は本ファイルを参照すること。
> このファイルが規約・手順の**唯一の正本（single source of truth）**である。

---

## 1. プロジェクト概要

**SafeG-M** = ARMv8-M TrustZone-M 上の**デュアルOSモニタ**。Secure 側で TOPPERS/ASP3 を走らせ、その上で Non-Secure(NS) OS（ベアメタル／FreeRTOS 等）を起動・監視する。

| 項目 | 内容 |
|---|---|
| アーキ | ARMv8-M Mainline + Security Extension（Cortex-M33。SG/BXNS/BLXNS・SAU・`*_NS` バンク・FPCCR_NS・BASEPRI_S/NS・AIRCR.BFHFNMINS を使用） |
| Secure OS | TOPPERS/ASP3（**外部リポジトリ `asp3_core` を参照**。CMake・非TECS・Python cfg・3.7.2 系） |
| NS OS | ベアメタル（`test/ns_baremetal/`）／ FreeRTOS（`FreeRTOS/`・`test/ns_freertos/`） |
| 構成方針 | **案1**：素の `asp3_core` に SafeG 改変を `#ifdef TOPPERS_SAFEG_M` ガードで取り込む。**既定 OFF で素 ASP3 不変＝非回帰**。M0 で設計凍結（`doc/M0_design_freeze.md`） |
| 遷移テスト | A〜D1 カテゴリ（`doc/test_safeg.md`）。**実機 i.MX RT685・RP2350 ともフル 9/9 PASS 達成済み** |

### 案1（現行の正）と旧構成
- **現行 = 案1**：Secure カーネル本体・SAFEG ボード層は `asp3_core` 側にあり、本リポジトリは **NS とテスト・FreeRTOS・doc・tools のみ**を持つ（極小化）。
- 旧構成（asp3_3.7 を丸ごと同梱した overlay 版＋tecsgen＋旧 TZ ポート lpc55s69/nrf5340/stm32l552/mimxrt685）は **2026-06-17 に削除**。必要なら git 履歴（`main`〜`m3-nontecs` 系列）を参照。

---

## 2. ⚠️ 禁則事項（作業前に必読）

1. **`asp3_core` の `kernel/`・`include/`・`library/` を編集しない**（上流 ASP3 追従領域。`asp3_core` 側 AGENTS.md §2 の禁則に従う）。
2. **SafeG 改変は必ず `#ifdef TOPPERS_SAFEG_M`（⇒ `TOPPERS_ENABLE_TRUSTZONE`）でガードする**。既定 OFF で素 ASP3 が一切変わらないこと（全工程で SAFEG=0 非回帰を維持）。
3. **カーネル内で動的メモリ確保（malloc 等）を使わない**（静的生成のみ。ISO 26262 / IEC 61508 方針）。

---

## 3. リポジトリ構成と配置

### ディレクトリ
```
test/secure/        Secure 側 遷移テスト本体（test_safeg.{c,h,cfg}・test_gate.c=CMSE gate群）
test/common/        kernel 非依存の test harness（test_harness.{c,h}・test_gate.h）。ボード非依存
test/ns_baremetal/  NS ベアメタル（ns_test_main.c 共通 + ボード別 ld/startup/Makefile）
                      mps2_an505_gcc / mimxrt685evk_gcc / pico2_arm_gcc
test/ns_freertos/   NS FreeRTOS（mimxrt685evk_gcc）
FreeRTOS/           FreeRTOS サンプル
tools/              import/CI スクリプト（import_asp3.sh・import_freertos.sh・ci_an505.sh）
doc/                設計・移植・実機 green 記録（索引は §7）
```

### workspace sibling 配置（重要・M0 §10-Q3 確定）
- `asp3_core` と `SafeG-M` を**同一ワークスペース直下に横並びで clone**する（submodule 不採用）。
- 別PCでは**両リポジトリを clone**すること。
- 版固定は submodule SHA の代わりに**使用 commit ペアを本ファイル §6 に記録**して再現性を担保。

---

## 4. ビルド & 実機テスト

> 前提：`asp3_core`（非TECS・CMake）は CMake+Ninja 必須。NS ベアメタルは `make` + arm-none-eabi-gcc。
> ツールチェーン・プローブ・シリアル番号は**マシン依存**。各機での実体は `doc/HANDOFF.md` §6 を参照。

### Secure（asp3_core, SAFEG=1 + CMSE implib）
`<board-preset>` は `pico2_arm`(RP2350) / `mimxrt685evk` / `mps2_an505-qemu`(非遷移CI) 等。
```bash
cmake --preset <board-preset> -B <build-dir> \
  -DENABLE_SAFEG_M=ON -DENABLE_SAFEG_IMPLIB=ON \
  -DASP3_APPLNAME=test_safeg \
  -DASP3_APPLDIR=<SafeG-M>/test/secure \
  -DASP3_EXTRA_APP_C_FILES="<SafeG-M>/test/secure/test_gate.c;<SafeG-M>/test/common/test_harness.c" \
  -DASP3_APP_INCLUDE_DIRS=<SafeG-M>/test/common
cmake --build <build-dir>     # → asp.elf（Secure本体）+ secure_nsclib.o（gate implib, gate veneer ×10）
```
- `ENABLE_SAFEG_IMPLIB`（既定 OFF）は **gate 保有アプリ時のみ** ON にする（gate 無アプリでは `--out-implib` が "no symbols" で失敗するため）。

### NS（ベアメタル, Secure を先にビルドして implib をリンク）
```bash
cd test/ns_baremetal/<board>_gcc
make NSCLIB=<build-dir>/secure_nsclib.o \
     EXTRA_CFLAGS="-DTST_ENABLE_A3 -DTST_ENABLE_C -DTST_ENABLE_D1"
# → nstest.elf / nstest.bin（NS code/RAM はボード別 ns_*.ld の ORIGIN）
```
- **`TST_ENABLE_A3/C/D1` は NS 側のみ有効**（既定は A1/A2/B1〜B4。Secure は常時全ハンドラ提供）。フル A〜D1 = pass 9。

### 実機取得（[TST] はワンショット同期出力）
1. **シリアル capture を reset より先に起動**（`stty -F <tty> 115200 raw -echo; cat <tty>`）。
2. 書込み後、**デバッガ非接続で自走**させる（常駐デバッガは D1 の NS udf→Secure HardFault を handler 前に halt して D1 が出ない）。
   - RP2350/openocd 例：`program asp.elf verify; program nstest.bin <NS_ORIGIN> verify; reset; exit`（`reset; exit` で切断＝自走）。
3. 期待出力：`[TST] ... PASS` 各 CHK → `[TST] SUMMARY total=9 pass=9 fail=0` → `[TST] DONE`。

### 役割分担（M0 確定）
- **RP2350(pico2_arm) = 遷移テストの主**、**imxrt685 = 回帰の保険**、**QEMU mps2-an505 = 非遷移系 CI 専用**（QEMU は SG/INVEP エミュ限界で遷移 green 不可）。

### 検証の鉄則
- 変更後は必ずビルドが通ることを確認してから報告する。実機結果は `SUMMARY total=N pass=N` を根拠にする。「動くはず」で報告しない。

---

## 5. 重要な発見・ハマりどころ
- **FPU 退避（最重要回帰項目）**：gate veneer の FP スクラッチで BTASK 文脈に `CONTROL_S.FPCA=1` が残り、横取りディスパッチ時の `vstmdb {s16-s31}` が HardFault。`asp3_core` の `core_support.S` で **BTASK は FP 退避/復帰をスキップ**して解消。ASP3 更新時は必ず C カテゴリで非回帰確認。
- **自走必須**：上記「実機取得」②。imxrt685・RP2350 とも同じ罠。
- **[TST] 同期出力**：`test_start()` で logmask から LOG_EMERG を除外し lowmask 即時出力（`target_fput_log`）のみ＝LogTask 非依存・1回・同期（`test/common/test_harness.c`）。
- **非TECS ガード**：`test/secure/test_safeg.cfg` は `#ifndef TOPPERS_OMIT_TECS` で分岐。非TECS（`TOPPERS_OMIT_TECS` 定義）では asp3_core のプレーン syssvc cfg（syslog/banner/serial/logtask）を使う。

---

## 6. 版固定（使用 commit ペア）

submodule を使わないため、動作確認した組み合わせをここに記録する（再現時はこの組で clone）。

| 日付 | SafeG-M | asp3_core | 実機 green |
|---|---|---|---|
| 2026-06-17 | `27c1a15` | `3b548ab` | imxrt685 9/9・RP2350 9/9（A〜D1） |

> 新しい組で green を取ったら**行を追加**すること（最新行が現行の正）。

---

## 7. ドキュメント索引（`doc/`）

| やりたいこと | 読むファイル |
|---|---|
| **別PC/別セッション再開** | **`doc/HANDOFF.md`**（最重要メモ・HW環境・残課題） |
| 案1 の設計凍結 | `doc/M0_design_freeze.md`（差分4分類・core_support.S 9サイト・EXC_RETURN matrix） |
| 遷移テスト仕様 | `doc/test_safeg.md`（[TST] 仕様・CP 表） |
| 実機 green 記録・再現手順 | `doc/asp3core_imxrt685_full_green.md`（imxrt685）・`doc/rp2350_bringup.md`（RP2350） |
| 各フェーズ設計 | `doc/m2_loadin_spec.md`・`doc/m3_nontecs_test.md` |
| 旧 SafeG-M ソース解析 | `doc/analysis_imxrt685.md`（launch_ns/deactivate/SAU を file:line で。**参照先 `asp3/` は削除済＝git 履歴を見る**） |
| 案比較（参考・SUPERSEDED） | `doc/asp3core_migration.md`（M0 が正） |

---

## 8. Git
- ブランチ：作業は `main` に統合済（旧 `m3-nontecs`）。`origin/main` と一致。
- コミット例：`<type>(<scope>): <summary>`（type=feat/fix/docs/test/chore/refactor）。
- 大きな構成変更・実機 green 取得時は `doc/` に記録を残す（HANDOFF を最新化）。
