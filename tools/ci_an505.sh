#!/usr/bin/env bash
#
# ci_an505.sh — SafeG-M 遷移テストを QEMU mps2-an505 で実行し pass/fail 判定する
#
# 前提:
#   - Secure テストアプリを SAFEG=1 でビルド済み:
#       cd asp3 && rm -rf build_an505_test && mkdir build_an505_test && cd build_an505_test
#       ruby ../configure.rb -T mps2_an505_gcc \
#         -a '$(SRCDIR)/../test/secure $(SRCDIR)/../test/common' \
#         -A test_safeg -U 'test_gate.o test_harness.o'
#       make ENABLE_SAFEG_M=1   # [+ EXTRA_CFLAGS で TST_ENABLE_A3/_C/_D1]
#   - NS ベアメタルをビルド済み:
#       cd test/ns_baremetal/mps2_an505_gcc && make   # [EXTRA_CFLAGS=...]
#   - qemu-system-arm (8.2.2+), arm-none-eabi-gcc が PATH 上にあること
#
# 使い方:  tools/ci_an505.sh
# 終了コード: 0=PASS(全CP pass & DONE), 1=FAIL, 2=実行/ビルド成果物なし
#
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SECURE="$ROOT/asp3/build_an505_test/asp"
NSBIN="$ROOT/test/ns_baremetal/mps2_an505_gcc/nstest.bin"
LOG="${LOG:-/tmp/ci_an505.log}"
NS_BASE="${NS_BASE:-0x00200000}"   # NS code エイリアス（Secure 0x10200000 と同一実体）
TIMEOUT="${TIMEOUT:-20}"

[ -f "$SECURE" ] || { echo "missing Secure image: $SECURE (先に SAFEG=1 でビルド)"; exit 2; }
[ -f "$NSBIN" ]  || { echo "missing NS image: $NSBIN (先に NS をビルド)"; exit 2; }

timeout "$TIMEOUT" qemu-system-arm -M mps2-an505 -cpu cortex-m33 -nographic \
    -semihosting-config enable=on,target=native \
    -kernel "$SECURE" \
    -device loader,file="$NSBIN",addr="$NS_BASE" \
    > "$LOG" 2>&1
# timeout(124) は想定内（DONE 後もカーネルが走り続けるため）。判定はログ内容で行う。

echo "----- [TST] transcript -----"
grep -aE "\[TST\]" "$LOG"
echo "----------------------------"

if ! grep -aq "\[TST\] DONE" "$LOG"; then
    echo "RESULT: FAIL (no [TST] DONE — テスト未完了/起動失敗)"; exit 1
fi
# A〜D の主要チェックポイントが揃っているか（"D1 だけ" の退行を PASS と誤判定しない）
#   NS 遷移が壊れると hardfault_handler 経由で D1 のみ記録され DONE/fail=0 になりうるため，
#   起動(A1)・gate(B1) の通過を必須とする．
for need in "CP A1" "CP B1"; do
    if ! grep -aq "\[TST\] $need" "$LOG"; then
        echo "RESULT: FAIL ([TST] $need が無い — A〜C 未実行。NS遷移が機能していない可能性)"; exit 1
    fi
done
# 最初の SUMMARY 行で判定
SUMMARY="$(grep -aE "\[TST\] SUMMARY" "$LOG" | head -1)"
echo "RESULT: $SUMMARY"
if echo "$SUMMARY" | grep -qE "fail=0"; then
    echo "RESULT: PASS"; exit 0
else
    echo "RESULT: FAIL"; exit 1
fi
