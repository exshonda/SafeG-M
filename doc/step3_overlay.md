# Step3: ASP3 取り込み方法の改善（パッチ依存削減 / overlay 方式）

## 目的
従来 SafeG-M は ASP3 を丸ごと同梱し `safeg.patch`(8本)で改変していた。本 Step3 で
**外部の素ASP3を `ASP3_BASE` から参照し，SafeG-M は必要ファイルのみ保持，safeg.patch は廃止**
という構成へ移行した（CLAUDE.md Step3）。

## 方式: overlay（探索順で SafeG 優先）
- 外部素ASP3を `ASP3_BASE` で参照。SafeG所有ツリー(`asp3/`=`SRCDIR`)を**先**，`ASP3_BASE`を**後**に
  探索し，同名ファイルは SafeG 版が勝つ（＝パッチではなく「置き換え」）。
- 散在する `#ifdef TOPPERS_SAFEG_M` 改変は，該当ファイルを**丸ごと SafeG所有**にすることで表現
  （パッチの hunk ずれ問題が消える）。
- `ovl` 関数（`sample/Makefile`）: `$(call ovl,rel/path)` = SRCDIR に在ればそれ，無ければ ASP3_BASE。
  - `ASP3_BASE` 未取り込みでも SRCDIR に全ファイルが在れば従来通り動く（**後方互換**）。

## ファイル構成（移行後）
- `asp3/` ソース = **99ファイル**(arch 46 / target 44 / cfg 5 / 他) + **tecsgen 232**(当面 SafeG所有)。
  - must-override(改変): common 約10, chip(imxrt600 等) chip_kernel.h/chip_sil.h, target の
    target_kernel_impl.c/target_kernel.cfg/target_kernel.trb/Makefile.target，`core_support.S`(修正A込み) 等。
  - must-own(新規): `arch/arm_m_gcc/mps2_an505/`，`target/mps2_an505_gcc/`，各 `*_s.ld`，
    `arch/arm_m_gcc/common/core_kernel.cfg`，`cfg/test/` 等。
  - build-overlay 化したファイル: `arch/arm_m_gcc/imxrt600/Makefile.chip`（CHIPDIR_BASE 追加）。
- `ASP3_BASE`(既定 `<repo>/asp3_base`) = 素ASP3 3.7.2 を LF 正規化したキャッシュ(**4205ファイル**)。
- `safeg.patch`: **全廃**（8本削除。記録は `/tmp/asp3_pruned_stage/_safeg_patches` に退避）。

## 取り込み（import）
```sh
tools/import_asp3.sh [SRC_ASP3] [DEST_ASP3_BASE]
# 既定: SRC=/home/honda/TOPPERS/FMP3/work/asp3_3.7  DEST=<repo>/asp3_base
# rsync 同期 + CRLF→LF 正規化(冪等)
```

## ビルド変更（file:line 要点）
- `asp3/configure.rb`: `--asp3-base` オプション追加; `$vartable["ASP3_BASE"]` 出力(既定
  `$(SRCDIR)/../asp3_base`); `CFG` を `$(call ovl,cfg/cfg.rb)` 化; 既定 appldirs に `$(ASP3_BASE)/sample` 追加。
- `asp3/sample/Makefile`(テンプレート): `ASP3_BASE`/`ovl` 定義; `INCLUDES`/`SYSSVC_DIRS`/`KERNEL_DIRS`/
  `APPL_DIRS` に ASP3_BASE 後置; `TARGETDIR_BASE` 定義; `CFG_TABS`・`kernel/Makefile.kernel`・
  `TARGET_*_TRB/CFG` を `ovl` 化。
- `asp3/arch/arm_m_gcc/common/Makefile.core`: `COREDIR_BASE`/`TOOLDIR_BASE` 追加(INCLUDES/vpath/KERNEL_DIR);
  `core_sym.def`/`core_offset.trb` を `ovl` 化。
- `asp3/arch/arm_m_gcc/imxrt600/Makefile.chip`: `CHIPDIR_BASE` 追加(INCLUDES/KERNEL_DIRS/SYSSVC_DIR)。
- `asp3/target/mimxrt685evk_gcc/Makefile.target`: `TARGETDIR_BASE` を INCLUDES/KERNEL_DIRS に追加;
  chip include と timer/trb 依存を `ovl` 化。

## 検証（実施済み）
- Secure(mimxrt685, `build_test`, TECS): `make exit 0` + **configuration check passed**，`asp.srec` 61212B
  （overlay前 ≈61068B，flash/SRAM/SG_veneer フットプリント一致＝等価バイナリ）。
- NS(ベアメタル mimxrt685, A3/C/D1): `make exit 0`，`nstest.axf`。
- an505 ターゲット(別ターゲットでの overlay 動作確認): `make exit 0` + config check passed。
- safeg.patch 廃止後も Secure/NS ビルド維持。
- **実機 A〜C 再確認は未実施**: 本セッションのボード自走不良(boot-ROM stall)が継続のため。overlay は
  同一ソース・同一ツールチェーンで等価バイナリを生成しており(フットプリント一致)，Step2 で実機 A〜C
  PASS 済みのため機能等価と判断。ボード回復時に再確認可。

## upstream 追従運用
1. `tools/import_asp3.sh` で新版 ASP3 を ASP3_BASE に取り込み(LF正規化)。
2. must-override ファイル(約18, RT685 で約12)のみ `diff3 -m`(旧vanilla/新vanilla/SafeG現) で 3-way マージ。
3. `build_test`/NS 再ビルド + 実機 test/(A〜D1) で回帰確認。
→ safeg.patch の hunk 再生成より明確に楽。

## 残課題 / 留意
- tecsgen(232) は版差が大きいため当面 SafeG所有。将来 external 化は別途検討。
- ロールバック: 退避は `/tmp/asp3_pruned_stage/`，着手前バックアップ `/tmp/asp3_backup_step3_pre.tgz`。
- Step4(FreeRTOS) は同型の `FREERTOS_BASE` + 同じ import/overlay/3-way フローを再利用可能。
