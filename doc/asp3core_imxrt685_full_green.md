# asp3_core ベース SafeG-M：実機 i.MX RT685 フル A〜D1 = 9/9 green（確定）

取得日: 2026-06-17 / ブランチ: asp3_core `safeg-m-imxrt685` + SafeG-M `m4-imxrt685`

## 結論
M0 凍結（案1：SafeG TZ を `#ifdef TOPPERS_SAFEG_M` で **asp3_core(CMake/非TECS/3.7.2)** へ取り込み）に基づく SafeG-M dual-OS が、**実機 i.MX RT685(mimxrt685evk) でフル遷移テスト A〜D1 を 9/9 PASS（fail=0, DONE）**。再現性あり（連続2周同一）。**QEMU の INVEP 限界は実機で発生せず**、FPU退避バグ（修正(A)）非回帰も C カテゴリ実行で確認（fault 0件）。

## 実機トランスクリプト（/dev/ttyACM1 @115200, RESET 自走・デバッガ非接続）
```
TOPPERS/ASP3 Kernel Release 3.7.2 for MIMXRT685-EVK
[TST] START prog=safeg-trans
[TST] RESULT_ADDR 0x30000020
[TST] CP A1 161 / CHK 1 PASS / CHK 2 PASS          (A1 起動到達, A2 CONTROL_NS=0/FAULTMASK=1)
[TST] CP A3 163                                     (BTASK ter_tsk→act_tsk 再起動)
[TST] CP B1 177 / CHK 3 PASS / CHK 4(0x80) PASS / CHK 5(0x0) PASS  (gate/API/BASEPRI 0x80→0)
[TST] CP B4 180                                     (gate 多重)
[TST] CP C1 193 / CHK 194 PASS / CHK 195 PASS / CHK 206 PASS  (Secureタイマ横取り→hi_task / BTASK状態 / NSマスク0x80)
[TST] CP D0 208
[TST] CP D1 209 / CHK 209 PASS / MARK 0xf004(HFSR.FORCED)     (NS udf→exc3 HardFault を Secure hardfault_handler が捕捉)
[TST] SUMMARY total=9 pass=9 fail=0 cp=7
[TST] DONE
```

## 取得手順（要点）
- ビルド: asp3_core CMake（`pico2_arm`/`mimxrt685evk_gcc`）, `ENABLE_SAFEG_M=ON`/`ENABLE_SAFEG_IMPLIB=ON`, app=test/secure + test/common/test_harness.c。NS は `test/ns_baremetal/mimxrt685evk_gcc` を `-DTST_ENABLE_A3 -DTST_ENABLE_C -DTST_ENABLE_D1`。`TST_ENABLE_*` は NS 側のみ有効（Secure は常時全ハンドラ提供）。
- 書込み: LinkServer（probe ISA0BQNQ, MIMXRT685S:EVK-MIMXRT685）。Secure `load asp.srec`, NS `load nstest.axf`(@0x08400000)。
- 実走/取得: **デバッガ非接続で RESET 自走**が必須（LinkServer run 等のデバッガ常駐だと NS の udf→HardFault を handler 前に halt して D1 が記録されない）。**capture を先に開始してから RESET**（[TST] はワンショット）。
- 出力: `[TST]` は同期出力（test_harness の logmask 調整で LogTask 非依存・即時 UART）。

## 留意（別ボード）
- **RP2350(Pico2)**: SAFEG=1 ファーム実走後に SWD デバッグがロックされる（セキュアデバッグ挙動）ため SWD 反復書込みが困難。実機フル取得は UF2/picotool（USBマスストレージ・SWD非依存）経路が要（doc/rp2350_bringup.md）。遷移自体は実機 RP2350 で NS launch 完走・INVEP 無しを確認済。
- QEMU(mps2-an505): SG/INVEP のエミュ限界で A〜D1 完走不可。ビルド/非遷移系 CI 用。遷移テストの正は実機（i.MX RT685 が確定基準、RP2350 は UF2 経路で追加予定）。
