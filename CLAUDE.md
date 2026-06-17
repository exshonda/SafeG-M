# CLAUDE.md

このプロジェクトの規約・手順は **AGENTS.md** を正本とします。
作業を始める前に必ず AGENTS.md を読んでください。

@AGENTS.md

---

## 特に重要（AGENTS.md より抜粋）

1. **`asp3_core` の `kernel/`・`include/`・`library/` を編集しない**（上流 ASP3 追従領域）。
2. **SafeG 改変は必ず `#ifdef TOPPERS_SAFEG_M` でガード**。既定 OFF で素 ASP3 不変＝非回帰。
3. **`asp3_core` と `SafeG-M` は workspace sibling で横並び clone**（submodule 不採用）。版固定の commit ペアは AGENTS.md §6。

## クイックスタート（RP2350 実機フル A〜D1）

```bash
# 1) Secure（asp3_core, SAFEG=1 + implib）
cmake --preset pico2_arm -B <build-dir> -DENABLE_SAFEG_M=ON -DENABLE_SAFEG_IMPLIB=ON \
  -DASP3_APPLNAME=test_safeg -DASP3_APPLDIR=<SafeG-M>/test/secure \
  -DASP3_EXTRA_APP_C_FILES="<SafeG-M>/test/secure/test_gate.c;<SafeG-M>/test/common/test_harness.c" \
  -DASP3_APP_INCLUDE_DIRS=<SafeG-M>/test/common
cmake --build <build-dir>
# 2) NS（ベアメタル, 全カテゴリ）
cd test/ns_baremetal/pico2_arm_gcc
make NSCLIB=<build-dir>/secure_nsclib.o EXTRA_CFLAGS="-DTST_ENABLE_A3 -DTST_ENABLE_C -DTST_ENABLE_D1"
```

詳細なビルド・実機取得・禁則・版固定はすべて AGENTS.md を参照。マシン依存の HW 環境は `doc/HANDOFF.md` §6。
