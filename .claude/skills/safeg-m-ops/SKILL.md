---
name: safeg-m-ops
description: SafeG-M リポジトリ固有の運用（ARMv8-M TrustZone Dual-OS: Secure=ASP3(asp3_core) / NS=FreeRTOS・ベアメタル）。asp3_core ベース（案1）で Secure+NS 遷移テスト(test/) を実機で回すとき＝test_safeg の CMake ビルド、NS の `TST_ENABLE_A3/_C/_D1`、2イメージ書込み、`[TST]` 取得・合否判定。実機 i.MX RT685 / RP2350(Pico2) で A〜D1 を取るとき、QEMU mps2-an505 の INVEP 限界・RP2350 の SAFEG=1 SWD-lock・**[TST]ワンショット＝capture先行＋デバッガ非接続で自走**・FPU退避(BTASK)回帰 に当たったときに使う。**規約・ビルド・実機手順の正本は repo の `AGENTS.md`（→`doc/HANDOFF.md`）。asp3_core 側の CMake/QEMU/OpenOCD は別skill `asp3-core-ops`、TOPPERS共通概念は `toppers-kernel-dev`/`toppers-kernel-debug`/`toppers-asp`。本skillは発動トリガと役割分担を示し、具体手順は AGENTS.md へ委譲する。**
---

# SafeG-M 運用（リポジトリ固有・委譲ハブ）

このリポは TrustZone-M Dual-OS（Secure=ASP3 on `asp3_core` / NS=FreeRTOS・ベアメタル）。SafeG 改変は **案1**（素 asp3_core に `#ifdef TOPPERS_SAFEG_M` 取込み・既定OFFで非回帰）。**具体手順は重複を避け、このリポの正本へ委譲する。**

## まず読む（正本）
- **`AGENTS.md`** … 規約・禁則・**ビルド/実機テストの具体コマンド**・役割分担・版固定・doc索引（全AIツール共通の正本）。`CLAUDE.md` は AGENTS.md を参照するだけ。
- **`doc/HANDOFF.md`** … 別PC/別セッション再開メモ・**マシン依存のHW環境(probe/tty/復旧)**・残課題。
- 設計凍結=`doc/M0_design_freeze.md`、実機green記録=`doc/asp3core_imxrt685_full_green.md`・`doc/rp2350_bringup.md`、テスト仕様=`doc/test_safeg.md`。

## 役割分担（他skillとの境界）
- **asp3_core 側の叩き方**（CMake preset・QEMU・OpenOCD・slog・TTSP3・DWT・`ENABLE_SAFEG_M`/`ENABLE_SAFEG_IMPLIB`・SAFEG層・素ビルド非回帰）→ 別リポ skill **`asp3-core-ops`** ＋ `asp3_core/docs/dev/safeg.md`。
- **TOPPERS共通概念**（実装非依存）→ `toppers-kernel-dev`（規約/移植/上流追従）・`toppers-kernel-debug`（症状→原因/観測/TZ遷移の着眼）・`toppers-asp`（API/静的API）。
- **本リポ固有の Dual-OS 遷移テスト運用** → 本skill＝**AGENTS.md §4/§5 と HANDOFF.md を指す**。

## このリポで繰り返し効く要点（詳細は AGENTS.md）
- ビルドは Secure(asp3_core CMake, `ENABLE_SAFEG_M=ON` ＋ gate保有時 `ENABLE_SAFEG_IMPLIB=ON`)→NS(`test/ns_baremetal/<board>_gcc`, `TST_ENABLE_A3/_C/_D1` は NS側のみ)の順（NS が `secure_nsclib.o` をリンク）。
- **実機取得は「シリアル capture を reset より先に起動」＋「デバッガ非接続で自走」**（常駐デバッガは D1 の NS udf→Secure HardFault を handler 前に halt して D1 が出ない。`[TST]` はワンショット）。判定＝`[TST] DONE && SUMMARY ... fail=0`。
- 役割: RP2350(pico2_arm)=遷移の主、imxrt685=保険、QEMU mps2-an505=非遷移CI（QEMUは SG/INVEP 限界で遷移green不可）。
- 最重要回帰=FPU退避（BTASKのFP退避skip, asp3_core `core_support.S`）。ASP3更新時は C カテゴリで非回帰確認。
- ビルド成果物(`nstest.*`/`*.axf`/`out.map`/直下`*.log`)は `.gitignore` 済（コミットしない）。
