---
name: safeg-m-ops
description: SafeG-M リポジトリ固有の運用（ARMv8-M TrustZone Dual-OS: Secure=ASP3 / NS=FreeRTOS等）。asp3_core ベース（案1）で Secure+NS の遷移テスト(test/) を実機で回すときの具体手順。test_safeg アプリの CMake ビルドレシピ、NS ベアメタル(test/ns_baremetal/<target>)の TST_ENABLE_A3/_C/_D1、2イメージ書込み、`[TST]` 機械可読出力の取得・合否判定を扱うとき。実機 i.MX RT685(LinkServer/ttyACM1) や RP2350(Pico2: openocd/ttyACM2) で A〜D1 遷移テストを取るとき、QEMU mps2-an505 の INVEP 限界・RP2350 の SAFEG=1 SWD-lock・imxrt685 の D1 デバッガhalt-on-fault(RESET自走+capture先行) に当たったときに使う。**正本は repo の `doc/HANDOFF.md`。asp3_core 側のビルド/QEMU/実機ロードは別skill `asp3-core-ops`、TOPPERS共通概念は `toppers-kernel-dev`/`toppers-kernel-debug`/`toppers-asp`。本skillは SafeG-M 固有の Dual-OS 遷移テストの叩き方だけを補う。**
---

# SafeG-M 運用（リポジトリ固有・Dual-OS 遷移テスト）

SafeG-M(TrustZone-M Dual-OS) を **外部の素 asp3_core(CMake/非TECS) に SafeG改変を `#ifdef TOPPERS_SAFEG_M` で取り込む「案1」**で運用するときの具体手順。概念・規約は他で正本化済み。ここは「このリポでの叩き方」に徹する。

## 正本・関連（先に読む）
- **このリポの正本**: `doc/HANDOFF.md`（現状/ブランチ/再現手順/残課題/HW環境を網羅）。設計凍結=`doc/M0_design_freeze.md`、実機9/9記録=`doc/asp3core_imxrt685_full_green.md`、各フェーズ設計=`doc/m2_loadin_spec.md`/`doc/m3_nontecs_test.md`/`doc/rp2350_bringup.md`、テスト仕様=`doc/test_safeg.md`。
- **asp3_core 側の具体手順**（CMake preset・QEMU・OpenOCD・slog・TTSP3・DWT）: 別リポ skill `asp3-core-ops`、`asp3_core/docs/dev/safeg.md`。
- **TOPPERS共通概念**（実装非依存）: `toppers-kernel-dev`（規約/移植/上流追従）・`toppers-kernel-debug`（症状→原因/観測/TZ遷移）・`toppers-asp`（API/静的API）。

## 0. 配置（workspace sibling）
asp3_core と SafeG-M を**横並び clone**。SafeG-M のビルドは asp3_core の CMake に `-DASP3CORE_DIR=<asp3_coreパス>` 相当で参照（submodule不採用）。asp3_core main に SafeG TZ＋各ボードSAFEG層、SafeG-M main に test/NS/doc。

## 1. Secure テストアプリ（test_safeg）ビルド（asp3_core CMake）
asp3_core 側で SAFEG有効＋gate implib＋アプリ=SafeG-M の `test/secure` を指定（裏取り済: 実機 i.MX RT685 で 9/9 取得時のレシピ）。

```bash
# <RT>=asp3_coreパス, <SM>=SafeG-Mパス, <preset>= mimxrt685evk | pico2_arm | mps2_an505-qemu
cmake -S <RT> -B build/safeg --preset <preset> \
  -DENABLE_SAFEG_M=ON -DENABLE_SAFEG_IMPLIB=ON \
  -DASP3_APPLNAME=test_safeg -DASP3_APPLDIR=<SM>/test/secure \
  -DASP3_EXTRA_APP_C_FILES="<SM>/test/secure/test_gate.c;<SM>/test/common/test_harness.c" \
  -DASP3_APP_INCLUDE_DIRS=<SM>/test/common
cmake --build build/safeg          # → asp.elf/asp.srec ＋ secure_nsclib.o(gate implib)
```
- `ENABLE_SAFEG_IMPLIB`（既定OFF）= **gate(`cmse_nonsecure_entry`)を持つアプリのみ**で `--out-implib=secure_nsclib.o` を最終ELFに付与（gate無 sample だと "no symbols" 失敗するため opt-in）。NS がこれをリンクする。
- `ENABLE_SAFEG_M=ON` 自体・対象ボード・素ビルド非回帰は `asp3-core-ops` / `asp3_core/docs/dev/safeg.md`。

## 2. NS ベアメタル ビルド（SafeG-M 側）
NS は `test/ns_baremetal/<target>_gcc/`（an505 / pico2_arm / mimxrt685evk）。**`TST_ENABLE_A3/_C/_D1` は NS 側のみ有効**（Secure は常時全ハンドラ提供）。`secure_nsclib.o` をリンク。NS_VTOR は imxrt685=0x8400000 / RP2350=0x10200000 / an505=0x00200000。

```bash
cd <SM>/test/ns_baremetal/mimxrt685evk_gcc
make EXTRA_CFLAGS="-DTST_ENABLE_A3 -DTST_ENABLE_C -DTST_ENABLE_D1"   # → nstest.axf/bin
```

## 3. 書込み＋[TST]取得（ボード別・**遷移の正は実機**）
`[TST]` は同期出力（`test_harness.c` が logmask 調整で LogTask 非依存・即時 UART）。判定= **`[TST] DONE` 到達 && `SUMMARY ... fail=0`**。CP: A(起動/復帰) B(gate) C(割込/ディスパッチ) D1(NS例外捕捉)。

### i.MX RT685（推奨・9/9 実績）
- 書込み: LinkServer（probe `ISA0BQNQ`, device `MIMXRT685S:EVK-MIMXRT685`）で Secure `load asp.srec`、NS `load nstest.axf`。
- 取得: **デバッガ非接続で RESET 自走が必須**（LinkServer run 等デバッガ常駐だと D1 の NS udf→HardFault を firmware の handler 前に halt して D1 が出ない）。**`cat /dev/ttyACM1`(115200) を先に開始してから RESET ボタン**（[TST] はワンショット）→ `SUMMARY total=9 pass=9 fail=0 / DONE`。
- `load` 後 boot-ROM stall で自走しないことあり→RESET/電源再投入で復旧。詳細は `doc/asp3core_imxrt685_full_green.md`。

### RP2350(Pico2)
- 接続: Pico debugprobe(CMSIS-DAP `2e8a:000c`)、openocd `interface/cmsis-dap.cfg`+`target/rp2350.cfg`+`transport select swd; adapter speed 5000`、VCOM `/dev/ttyACM2`。
- ⚠️ **SAFEG=1 ファーム実走後に SWD デバッグがロック**（セキュアデバッグ挙動）→ SWD反復書込み困難。実機フル取得は **BOOTSEL＋UF2/picotool（USBマスストレージ・SWD非依存）経路が必要**（残課題、`doc/rp2350_bringup.md`）。劣化時は **BOOTSEL(押しながら電源)で復旧**。遷移自体は実機RP2350でNS launch完走・INVEP無しを確認済。

### QEMU mps2-an505（実機なし・**非遷移CIのみ**）
- `asp3-core-ops` の QEMU手順＋NSを `-device loader,file=<ns.bin>,addr=0x00200000`。⚠️ **最初の Secure gate で QEMU 固有 SecureFault `SFSR.INVEP`**（8.2.2/11.0.1で再現、実機では出ない）＝SG/Secure例外エミュ限界で A〜D1 完走不可。ビルド健全性/非遷移系の確認に使う。

## 4. このリポの落とし穴（実機運用）
- 全 LinkServer/openocd/シリアルは `timeout` で囲む（過去にハング多発）。
- ビルド成果物 `nstest.*`/`out.map`/直下 `*.log` は `.gitignore` 済（コミットしない）。
- `[TST]` ワンショット＝**capture を先に開始してから RESET**（後追い cat だと取り逃す）。
- FPU退避バグ修正(A)（`core_support.S` で BTASK の FP退避/復帰スキップ＝asp3_core側）は ASP3更新時の最重要回帰。C カテゴリ実行で非回帰確認。詳細は `asp3-core-ops`/`asp3_core/docs/dev/safeg.md`。
