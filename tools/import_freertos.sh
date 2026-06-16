#!/bin/sh
# import_freertos.sh - 外部の素 FreeRTOS-Kernel を FREERTOS_BASE キャッシュへ取り込み，改行をLFに正規化する．
#
# SafeG-M は FreeRTOS カーネル本体を同梱せず，SafeG 固有(sample/ と test NS)のみを保持し，
# 無改変のカーネル(tasks.c/queue.c/include/portable 等)はこの FREERTOS_BASE から
# オーバーレイ参照する(Step3 の ASP3_BASE と同型)．
#
# usage: tools/import_freertos.sh [SRC_DIR] [DEST_FREERTOS_BASE_DIR]
#   SRC_DIR    素 FreeRTOS-Kernel の場所. 省略時は V10.3.1-kernel-only を git clone して使用.
#   DEST_DIR   正規化キャッシュの出力先. 既定: <repo>/freertos_base
# 冪等: 繰り返し実行しても安全(rsync 同期 + LF 正規化).
#
# 対応 FreeRTOS 版: V10.3.1 (tag: V10.3.1-kernel-only)
# 注意: 同梱カーネルは V10.3.1 vanilla と機能的に同一(差分は include/timers.h のコメント文言のみ)．

set -e
FRTOS_TAG="V10.3.1-kernel-only"
FRTOS_REPO="https://github.com/FreeRTOS/FreeRTOS-Kernel"
HERE=$(cd "$(dirname "$0")/.." && pwd)
DEST="${2:-$HERE/freertos_base}"
SRC="$1"

CLONED=""
if [ -z "$SRC" ]; then
  SRC=$(mktemp -d)
  CLONED="$SRC"
  echo "import_freertos: cloning $FRTOS_REPO ($FRTOS_TAG)"
  git clone --depth 1 --branch "$FRTOS_TAG" "$FRTOS_REPO" "$SRC"
fi

if [ ! -d "$SRC" ]; then echo "import_freertos: source not found: $SRC" >&2; exit 1; fi
mkdir -p "$DEST"

echo "import_freertos: syncing $SRC -> $DEST"
rsync -a --delete --exclude '.git' "$SRC"/ "$DEST"/

echo "import_freertos: normalizing CRLF -> LF (text files only)"
find "$DEST" -type f \
  ! -name '*.srec' ! -name '*.o' ! -name '*.a' ! -name '*.bin' ! -name '*.elf' \
  ! -name '*.png' ! -name '*.jpg' ! -name '*.gif' ! -name '*.pdf' ! -name '*.zip' \
  -exec grep -Iq . {} \; -print0 2>/dev/null \
  | xargs -0 -r sed -i 's/\r$//'

[ -n "$CLONED" ] && rm -rf "$CLONED"
echo "import_freertos: done. FREERTOS_BASE=$DEST"
