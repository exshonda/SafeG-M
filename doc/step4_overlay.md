# Step4: FreeRTOS の取り込み方法の改善（FREERTOS_BASE オーバーレイ）

Step3（ASP3 の ASP3_BASE オーバーレイ）と同型で、Non-secure 側 FreeRTOS も
**外部の素 FreeRTOS-Kernel を参照し、SafeG-M は固有ファイルのみ保持**する構成へ移行した。

## 方式
- 外部の素 FreeRTOS-Kernel **V10.3.1**（tag `V10.3.1-kernel-only`）を `FREERTOS_BASE`（既定 `<repo>/freertos_base`）で参照。
- SafeG-M が所有するのは **`FreeRTOS/sample/`（SafeG 固有: main.c の Secure gate 呼出し・makedefs・各ターゲットの Makefile/standalone.ld/startup.c/FreeRTOSConfig.h・syscalls.c・sysmem.c）** と **`test/ns_baremetal/` `test/ns_freertos/`** のみ。
- カーネル本体（tasks.c/queue.c/list.c/timers.c/event_groups.c/croutine.c/stream_buffer.c・include/・portable/GCC/ARM_CM33_NTZ/non_secure・portable/MemMang/heap_4.c）は `FREERTOS_BASE` から overlay 供給。

## 素であることの検証（重要）
同梱カーネルを vanilla V10.3.1-kernel-only と LF 正規化後に diff:
- カーネル .c（7本）・port（port.c/portasm.c/portasm.h/portmacro.h）・heap_4.c・include/ の大半: **完全一致（差分0）**。
- **`include/timers.h` のみ 10 行差**＝**コメント文言のみ（機能差なし）**。overlay では vanilla 版を採用（機能不変）。
- vanilla 側の余分は `stdint.readme` のみ。
→ 同梱 FreeRTOS カーネルは **V10.3.1 vanilla と機能的に同一**。SafeG 改変は `sample/` に限られる（`main.c` の gate 呼出し、`sysmem.c` の `#include <sys/types.h>`=gcc13.2対応）。

## ビルド変更（探索順: SafeG sample 優先, カーネルは FREERTOS_BASE）
- `FreeRTOS/sample/{mimxrt685evk_gcc,lpc55s69evk_gcc,nucleo_l552zeq_gcc}/Makefile`:28-29
  `FREERTOS_BASE ?= ../../../freertos_base` / `RTOS_SOURCE_DIR=$(FREERTOS_BASE)`（従来 `RTOS_SOURCE_DIR=../../`）。`DEMO_SOURCE_DIR=../`(sample) は SafeG 所有のまま。
- `test/ns_freertos/mimxrt685evk_gcc/Makefile`:16-17
  `FREERTOS_BASE ?= ../../../freertos_base` / `RTOS_DIR = $(FREERTOS_BASE)`（従来 `../../../FreeRTOS`）。`FRTOS_SMP`(sample) は SafeG 所有のまま。
- `test/ns_baremetal/*` は FreeRTOS カーネル不使用（sample の startup/syscalls/sysmem のみ）＝**変更不要**。

## import / 追従運用
- `tools/import_freertos.sh [SRC] [DEST]`: SRC 省略時は `V10.3.1-kernel-only` を git clone。rsync + CRLF→LF 正規化（冪等）。既定 DEST=`<repo>/freertos_base`。
- FreeRTOS 更新時: import で `FREERTOS_BASE` を差し替え → 必要なら `sample/` 側のみ追従 → NS 再ビルド。**カーネルに SafeG 改変が無いため、ASP3 のような must-override 3-way マージは原則不要**（最も追従が軽い）。

## 検証結果
- Phase1（overlay 配線、削除前）: `test/ns_freertos`(FreeRTOS実体使用) `make` exit 0。
- Phase2（素カーネル剪定: FreeRTOS から 30 ファイル退避、sample のみ 47 ファイル残）:
  - `test/ns_freertos/mimxrt685evk_gcc` `make` exit 0、`nstest.axf` = **30396B（剪定前と一致＝等価）**。
  - `test/ns_baremetal/mimxrt685evk_gcc`（A3/C/D1）`make` exit 0、`nstest.axf` = **14704B（一致）**。
- カーネルは `../../../freertos_base/` からコンパイルされることをビルドログで確認。
- 残参照チェック: 削除済みカーネルへの参照は Makefile 類に**なし**。

## 構成（移行後）
- `FreeRTOS/` = **sample のみ 47 ファイル**（従来 77）。カーネル 30 ファイルは外部参照化。
- `freertos_base/` = 素 FreeRTOS-Kernel V10.3.1（518 ファイル、うち使用は CM33_NTZ port + MemMang + kernel + include）。
- **FreeRTOS 側にパッチ機構は元々無し**（ASP3 の safeg.patch のような廃止対象は存在しない）。

## バックアップ / ロールバック
- 着手前: `/tmp/freertos_backup_step4_pre.tgz`（FreeRTOS + test/ns_* 全体）
- 剪定退避（復元可）: `/tmp/freertos_pruned_stage/`（カーネル 30 ファイル）

## 残課題 / 留意
- **実機 A〜C 再確認は未実施**（本セッション継続のボード boot-ROM stall で自走時 UART 無出力）。NS バイナリは剪定前と**フットプリント一致＝等価**、Step2 で実機 A〜C PASS 済みのため機能等価と判断。ボード回復時に再確認可。
- `FreeRTOS/sample` の RTOSDemo リンクには Secure **サンプル**ビルドの `secure_nsclib.o`(`nonsecure_task`) が必要（現状は test gate 版が配置）。これは Step4 と無関係の既知の implib 共有問題。
- `FreeRTOS/sample/mps2_an505_gcc` は QEMU 作業由来の補助ディレクトリ（Makefile 無し）。
- Step3 と Step4 で `import_*.sh`・overlay・追従フローを共通化済み。`freertos_base/`・`asp3_base/` はリポジトリ管理外（.gitignore 推奨）。
