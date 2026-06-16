# Step2: ASP3 カーネル 3.7.0 → 3.7.2 更新 記録

対象: SafeG-M 同梱 `asp3/`（Release 3.7.0）→ Release 3.7.2
更新元: `/home/honda/TOPPERS/FMP3/work/asp3_3.7`（Release 3.7.2, 素のASP3）
戦略: (a) 3.7.2 を取り込み + SafeG 改変を保持/再マージ + 修正(A) を patch へ統合。
バックアップ: `/tmp/asp3_backup_3.7.0`（更新前 asp3 全体, 25MB）。
版確認: `include/kernel.h` `TKERNEL_PRVER = 0x3072`（更新後）。

## 重要な前提
- **CRLF/LF**: upstream 3.7.2 は CRLF、SafeG 同梱は LF。取り込み時に全ファイル LF 正規化（`tr -d '\r'`）。
- **修正(A) は safeg.patch 未記録だった**: `core_support.S` の BTASK FPU 退避/復帰スキップ（`dispatch_btask_nofp` 等）は `common/safeg.patch` に無い手動編集だった。本更新で patch へ統合済み（下記）。

## 更新方法（`arch/arm_m_gcc/common` と共有部の25差分ファイルを3分類）
LF 正規化後に「現ツリー(3.7.0+SafeG) vs 3.7.2」を比較し、`safeg.patch` 逆適用で vanilla 3.7.0 を再構成して upstream 変更を分離。

1. **3.7.2 を上書き（SafeG非依存・15ファイル）**: `kernel/{time_event.c,time_event.h,cyclic.c,kernel.trb,interrupt.trb,kernel_check.trb,kernel_sym.def}`, `cfg/{cfg.rb,pass1.rb,pass2.rb}`, `include/kernel.h`, `syssvc/{tBannerMain.c,tLogTaskMain.c}`, `arch/arm_m_gcc/common/{core_asm.inc,start.S}`。
   - 主な内容: タイムイベントヒープのインデックス化、割込み番号制限の解除、start.S の hook 追加等。
2. **3-wayマージ（SafeG改変 ∩ upstream変更・4ファイル）**: `core_kernel_impl.h`（bool キャスト+コメント）, `core_rename.def/.h`, `core_unrename.h`（`lock_cpu`/`unlock_cpu`/`sense_lock` のリネーム追加）。`diff3 -m` でクリーンマージ。
3. **現状維持（upstream変更なし or SafeG固有・6ファイル）**: `arm_m.h`, `core_kernel_impl.c`, `core_kernel.trb`, `Makefile.core`, `core_support.S`（修正(A)込み）, `core_insn.h`（`SCS_SYNC` は SafeG 側にあり 3.7.2 には無い／機能差は著作権・Id のみ）。

注: chip/target の 7 本の safeg.patch は upstream 3.7.2 で当該ファイルに変更が無いため再適用不要（現ツリー保持）。

## 修正(A) の safeg.patch 統合（landmine 解消）
- `arch/arm_m_gcc/common/safeg.patch` を **vanilla-3.7.2 基準で再生成**（`diff(vanilla3.7.2, 現ツリー)`）。
- 旧 patch は `/tmp/asp3_backup_3.7.0_common_safeg.patch.orig` に保存。
- 検証: 再生成 patch を vanilla-3.7.2 へ適用 → 現ツリーを **完全再現（差分0, exit 0）**。`grep dispatch_btask_nofp safeg.patch` = 2（修正(A) が patch に記録された）。

## ビルド（mimxrt685evk_gcc）
- Secure（test_safeg, TECS）: `asp3/build_test` で `make` → **exit 0, "configuration check passed"**。FLASH 20KB / SG_veneer 96B / SRAM 30KB。`secure_nsclib.o` 再生成。
- NS（ベアメタル）: `test/ns_baremetal/mimxrt685evk_gcc` で `make EXTRA_CFLAGS="-DTST_ENABLE_A3 -DTST_ENABLE_C -DTST_ENABLE_D1"` → **exit 0**, `nstest.axf`。

## 実機検証（i.MX RT685, LinkServer run で自走）
書込み: Secure `load`（verify で board==asp.srec 一致確認）、NS `load`。実行は `LinkServer run` で自走させ `/dev/ttyACM1` 115200 を捕捉。

結果（3.7.2）:
- **A1, A2, A3, B1, B2, B3, B4, C1, C2, C3 すべて PASS**（CHK 1..5,194,195,206 全 PASS）。
- **fault / Excno / HardFault = 0 件、`no time event in hrt interrupt` = 0 件**（タイマ健全）。
- D0(208) 到達。**回帰なし**を確認。

### D1 と最終 SUMMARY/DONE について（未捕捉・制約）
- `LinkServer run` はデバッガ接続実行のため、D1 の**意図的 CPU 例外（NS `udf`→exc3）でデバッガが vector-catch 停止**し、出力が D0/D1 で止まる（デバッガ由来であってボード停止ではない）。
- D1 を完走させ最終 `SUMMARY total=9 pass=9 fail=0 / DONE` を得るには **デバッガ非接続の自走（物理リセットでフラッシュ起動）** が必要。本環境ではボードが `load` 後に boot ROM stall で自走せず、自走には RESET ボタン押下が要る（本日の既知制約）。
- 補強: `DEF_EXC(HARDFAULT_EXCNO=3, hardfault_handler)` は 3.7.2 の configurator でも受理（config check 通過）。D1 機構は 3.7.0 で PASS 実績あり。→ 自走確認で D1 PASS 見込み高。

## 最終ステータス（2026-06-16, ユーザー判断: Step2 完了扱い）
- **Step2 は完了**。版 0x3072 更新・A〜C 全 PASS・回帰なし・タイマ健全・修正(A) を safeg.patch に正式記録・ビルド clean を確認済み。
- **D1 の 3.7.2 自走確認は未捕捉のまま保留（既知良）**。完全電源再投入＋RESET＋`load` 再フラッシュを試みたが、本ボードは `load` 後に boot ROM stall で自走せず UART 出力が出ない環境問題が継続（ASP3 更新とは無関係）。Secure の `load` 差分が 0x60 バイトのみだった点も自走形への書換え不全の疑い。
  - D1 機構は 3.7.0 で 9/9 PASS 実績あり＋3.7.2 configurator が `DEF_EXC(3)` 受理済み → 3.7.2 でも PASS 見込み高。ボードが自走可能な時に再確認する（任意）。
- 表示上の既知の軽微バグ（2周目以降 CP 名が前周末尾名のまま＝合否に無影響）。

## 変更ファイル一覧（asp3 配下, 本更新）
- 上書き(15) / マージ(4) / 現状維持(6): 上記「更新方法」参照。
- `arch/arm_m_gcc/common/safeg.patch`: 修正(A) 統合のため再生成（vanilla-3.7.2 基準）。
- 新規ファイル・mimxrt685/an505 ターゲット・test/ は不変。
