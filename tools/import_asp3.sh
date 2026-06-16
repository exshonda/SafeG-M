#!/bin/sh
# import_asp3.sh - 外部の素ASP3カーネルを ASP3_BASE キャッシュへ取り込み，改行をLFに正規化する．
#
# SafeG-M は ASP3 を丸ごと同梱せず，必要ファイル(must-own/must-override)のみを asp3/ に保持し，
# 無改変ファイル(external)はこの ASP3_BASE キャッシュからオーバーレイ参照する(safeg.patch を廃止)．
#
# usage: tools/import_asp3.sh [SRC_ASP3_DIR] [DEST_ASP3_BASE_DIR]
#   SRC_ASP3_DIR        素ASP3(upstream)の場所. 既定: /home/honda/TOPPERS/FMP3/work/asp3_3.7
#   DEST_ASP3_BASE_DIR  正規化キャッシュの出力先. 既定: <repo>/asp3_base
# 冪等: 繰り返し実行しても安全(rsync 同期 + LF 正規化).

set -e
SRC="${1:-/home/honda/TOPPERS/FMP3/work/asp3_3.7}"
HERE=$(cd "$(dirname "$0")/.." && pwd)
DEST="${2:-$HERE/asp3_base}"

if [ ! -d "$SRC" ]; then echo "import_asp3: source not found: $SRC" >&2; exit 1; fi
mkdir -p "$DEST"

echo "import_asp3: syncing $SRC -> $DEST"
rsync -a --delete "$SRC"/ "$DEST"/

echo "import_asp3: normalizing CRLF -> LF (text files only)"
# バイナリ(.srec/.o/.a/画像等)を除外してテキストのみ CR を除去
find "$DEST" -type f \
  ! -name '*.srec' ! -name '*.o' ! -name '*.a' ! -name '*.bin' ! -name '*.elf' \
  ! -name '*.png' ! -name '*.jpg' ! -name '*.gif' ! -name '*.pdf' ! -name '*.zip' \
  -exec grep -Iq . {} \; -print0 2>/dev/null \
  | xargs -0 -r sed -i 's/\r$//'

echo "import_asp3: done. ASP3_BASE=$DEST"
