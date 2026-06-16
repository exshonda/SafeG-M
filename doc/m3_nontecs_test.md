# M3 — 遷移テスト(test/)の非TECS化・asp3_core ビルド載せ替え 仕様

最終更新: 2026-06-17
ステータス: **設計（凍結前）**。本書は設計のみ。実コードは書かない。test/ ソースは未改変。

## 0. 位置づけ・根拠

- 前提: M0 設計凍結 `doc/M0_design_freeze.md` §10-Q4（**非TECS化で確定**）。
  - `test_safeg.cfg` の `INCLUDE("tecsgen.cfg")` を除去。
  - `test_safeg.cdl`（serial/syslog/banner/logtask セル）を**廃止**。
  - 接続先は asp3_core 非TECS syssvc（`syssvc/{serial,logtask,banner,syslog}.c` + `cmsdk_uart.c`(an505)）。
  - `tSysLogAdapter`/`tSerialAdapter` は非TECSで消滅、C から `syslog()`/serial を直接呼出し。
  - 残確認（本書で実施）: `test_harness.c` / `test_gate.c` が TECS 生成エントリを呼んでいないか。NS 側は無改変。
- 進行順（M0 §10-Q2）: QEMU(mps2-an505・非遷移CI) → 実機(RP2350/imxrt685・遷移A〜D1)。
- 本タスクは**読み取り解析のみ**。成果物は本 doc 新規のみ。test/ ソースは編集しない（他作業非競合）。

### M3 が依存する上流（M1/M2）の前提と未充足項目（★重要）

非TECS化（M3）は cfg/CMake の配線変更であり、**遷移テストが実際に通る前提として M1/M2 のアーチ載せ替えが必要**。現状（2026-06-17 実測）の asp3_core 側充足度:

| 依存項目 | 現状 (file:line) | M3 にとっての扱い |
|---|---|---|
| `ENABLE_SAFEG_M` → `-DTOPPERS_SAFEG_M`/`-DTOPPERS_ENABLE_TRUSTZONE` | **済**: `arch/arm_m_gcc/common/arch.cmake:35-39` | M3 はこのオプションを ON にして使う |
| `_SAFEG_BTASK`（BTASK タスク）の cfg 生成 | **未**: SafeG 側 `arch/arm_m_gcc/common/core_kernel.cfg:7` にあるが asp3_core 側 `arch/arm_m_gcc/common/core_kernel.py` には**無し**（M1/M2 で `launch_ns`/BTASK 載せ替え時に追加されるべき） | M3 の前提（test_safeg.c が `_SAFEG_BTASK` を参照: test_safeg.c:57,75,96,118 等）。**未充足なら M3 はビルド不能** |
| CMSE gate（`-mcmse`）・`secure_nsclib.o`(CMSE import lib) の生成 | **未**: asp3_core CMake/arch.cmake に `-mcmse`/implib 生成は無し（要・別途確認＝サブエージェントへ依頼） | NS リンクに必須。M3 で CMake へ追加が必要（§5） |
| `core_support.S` の SafeG 9サイト + 修正(A) FP退避スキップ | M1/M2 載せ替え範囲（M0 §5） | M3 の遷移(C/D1) PASS の前提 |

→ **M3 のうち「QEMU 非遷移（Secure 単独・[TST] 出力の枠組み）」は M1 完了後すぐ着手可**。
**「実機 遷移(A〜D1)」は M2（BTASK/launch_ns/CMSE/FP修正）完了が前提**。本書はこの2段で工程を分ける。

---

## 1. TECS 依存の全棚卸し（実測 file:line）

### 1.1 `test/secure/test_safeg.cfg`（5項目）

| 行 | 記述 | TECS 依存か | 非TECS での扱い |
|---|---|---|---|
| `:4` | `INCLUDE("tecsgen.cfg");` | **TECS**（tecsgen 生成の cfg 断片を取り込む） | **書換え**。asp3_core 非TECS syssvc cfg の直接 INCLUDE へ（§2.3） |
| `:8-11` | `CRE_TSK(MAIN/RESTART/HI/EXC_TASK …)` | 非TECS（標準静的API） | **そのまま**（asp3_core Python cfg が CRE_TSK を解釈。記法同一） |
| `:13` | `CRE_CYC(TICK_CYC, …)` | 非TECS | **そのまま** |
| `:16` | `DEF_EXC(CPUEXC1, …)`（`#ifdef CPUEXC1`） | 非TECS | **そのまま**（CPUEXC1 は core 由来マクロ。EXCNO 検証は §2.4） |
| `:24` | `DEF_EXC(HARDFAULT_EXCNO=3, …)` | 非TECS | **そのまま**（D1 用。EXCNO 3 は asp3_core `core_kernel.py:23` の `EXCNO_VALID` に含まれる＝合法） |

→ **CRE_TSK/CRE_CYC/DEF_EXC は非TECS標準静的APIで、asp3_core Python cfg がそのまま解釈する**（記法差なし。sample1.cfg:16-29 と同型）。**移植が必要なのは `:4` の1行のみ**。

### 1.2 `test/secure/test_safeg.cdl`（全体廃止）

`import(<kernel.cdl>)`/`tSerialPort`/`tSerialAdapter`/`tSysLog`/`tSysLogAdapter`/`tLogTask`/`tBanner` の各セル（cdl 全行 `:9-122`）は **tecsgen 専用**。非TECSでは全廃。

- `SysLogAdapter`(cdl:39-41)/`SerialAdapter`(cdl:51-53): C↔TECS 橋渡しセル → **非TECSで消滅**。C は `syslog()`/serial API を直接呼ぶ。
- `SysLog`(cdl:64-72)/`SerialPort1`(cdl:82-89)/`LogTask`(cdl:97-110)/`Banner`(cdl:118-122): 非TECS syssvc cfg（`syslog.cfg`/`serial.cfg`/`logtask.cfg`/`banner.cfg`）が等価機能を ATT_INI/CRE_TSK で提供（§2.1）。

→ **`test_safeg.cdl` はファイルごと廃止（参照を断つ。物理削除は M3 実装フェーズ。本書では未改変）**。

### 1.3 `test/common/test_harness.c` / `.h` / `test_gate.h` / `secure/test_gate.c`

tecsgen 生成エントリ（`tSysLogAdapter_*` / `tSerialAdapter_*` 等）を**呼んでいない**ことを実測確認:

| ファイル | syslog/serial 呼出し (file:line) | TECS 生成シンボル参照 | 判定 |
|---|---|---|---|
| `test_harness.c` | `#include <t_syslog.h>` `:9`, `#include "syssvc/syslog.h"` `:10`; `syslog(LOG_EMERG,…)` `:47,48,64,78,86,93,101,122,126` | 無 | **非TECSで無改変動作**。`syslog()` は非TECS版でも同一の C マクロ/関数（asp3_core `syssvc/syslog.h:54` が `<t_syslog.h>` を取込む＝同一API） |
| `test_harness.h` | `#include <t_syslog.h>` `:14`, `#include "syssvc/test_svc.h"` `:15`, `#include "test_gate.h"` `:16` | 無 | **無改変**。`test_svc.h`(check_point/check_assert 等)は asp3_core `syssvc/test_svc.h` に同名存在 |
| `test_gate.c` | `#include "syssvc/syslog.h"` `:10`; `wup_tsk/get_tid/stp_cyc` 等カーネルAPI; `check_point/tst_chk_u32/tst_mark/tst_done`(ハーネスAPI) | 無 | **無改変** |
| `test_gate.h` | `<stdint.h>` のみ（`:13`。kernel非依存契約） | 無 | **無改変**（NS共有のため） |

→ **結論: ハーネス/gate は最初から「非TECS互換」に書かれている**（test_harness.c:5-6 コメント「TECS は使わない」が明示）。**[TST] 機械可読出力（START/RESULT_ADDR/CP/CHK/MARK/FAIL/SUMMARY/DONE）は非TECSでも同一に動く**（出力は全て `syslog(LOG_EMERG,…)` 経由＝同期低レベル出力。doc/test_safeg.md の出力仕様と完全一致）。

---

## 2. asp3_core 非TECS syssvc への接続方法（対応表）

### 2.1 TECSセル → 非TECS接続先 対応表

| 旧 TECS（test_safeg.cdl） | 機能 | 非TECS 接続先（asp3_core） | 初期化方式 |
|---|---|---|---|
| `tSysLogAdapter`(cdl:39) | C→syslog 橋渡し | **消滅**。C が `syslog()` 直接呼出し | — |
| `tSerialAdapter`(cdl:51) | C→serial 橋渡し | **消滅**。C が serial API 直接呼出し（テストは serial 直呼びしない） | — |
| `tSysLog`(cdl:64) | システムログ機能 | `syssvc/syslog.c` + `syssvc/syslog.cfg` | `ATT_INI({…, syslog_initialize})`（syslog.cfg:8） |
| `tSerialPort`(cdl:82) | シリアルドライバ | `syssvc/serial.c`+`serial_cfg.c` + `serial.cfg` | `ATT_INI({…, serial_initialize})`+`CRE_SEM`×N（serial.cfg:12-15） |
| `tLogTask`(cdl:97) | ログ出力タスク | `syssvc/logtask.c` + `logtask.cfg` | `CRE_TSK(LOGTASK,…)`+`ATT_TER`（logtask.cfg:8-10） |
| `tBanner`(cdl:118) | 起動メッセージ | `syssvc/banner.c` + `banner.cfg` | `ATT_INI({…, print_banner})`（banner.cfg:9） |
| `tSIOPort`(target.cdl 経由) | SIO 実体 | an505: `target/mps2_an505_gcc/{target_serial.c,cmsdk_uart.c}` | `target_serial.cfg`（CFG_INT/CRE_ISR/ATT_TER。an505:10-11） |

→ syssvc C ソースは CMake の `asp3_add_syssvc()`（`asp3_core.cmake:50-65`）がリンク。SIO 実体は target.cmake の `ASP3_SYSSVC_TARGET_C_FILES`（an505: target.cmake:91-94）。**M3 で test 側に C を足す必要は無い**（ハーネスの log_output/vasyslog 等も asp3_add_syssvc が供給。現 Makefile が手動列挙していた `log_output.o vasyslog.o t_perror.o strerror.o`(build_an505_test/Makefile:229) は asp3_core.cmake:59-64 で自動供給）。

### 2.2 syslog API の同一性（直接呼出しの根拠）

- 現ハーネス: `syslog(LOG_EMERG, …)`（test_harness.c:47 等）。`syslog`/`t_syslog` は **TECS版も非TECS版も同じ C インタフェース**（`syssvc/syslog.h` が `<t_syslog.h>` を取込む点は両 repo 同一: asp3_core `syssvc/syslog.h:53-54`）。
- 非TECS版では `vasyslog.c`/`log_output.c`（asp3_add_syssvc が供給）が `syslog()` の実体を提供。**ハーネスのコード変更は不要**。

### 2.3 `INCLUDE("tecsgen.cfg")` の置換（2案）

asp3_core の CMakeLists.txt は **`INCLUDE("tecsgen.cfg")` を生かしたまま非TECS syssvc へ展開するスタブを自動生成する**（CMakeLists.txt:86-97）:
```
generated/tecsgen.cfg = INCLUDE syslog.cfg / banner.cfg / serial.cfg
```
ただし**このスタブは `logtask.cfg` を含まない**（sample1.cfg は別途 `INCLUDE("syssvc/logtask.cfg")` を `#else` で持つ: sample1.cfg:9-12）。test は LogTask を使う（test_safeg.h:17 が `LogTask(3)` 前提、cdl:97 に tLogTask）。

- **案A（推奨・低リスク）**: test_safeg.cfg を sample1.cfg と同型（`#ifndef TOPPERS_OMIT_TECS … #else INCLUDE syslog/banner/serial/logtask.cfg #endif`: sample1.cfg:6-13）に書換える。非TECSビルドは `TOPPERS_OMIT_TECS` 定義済み（asp3_core CMakeLists.txt:54-56 が `TOPPERS_OMIT_TECS` を常時付与）なので `#else` 側＝4つの syssvc cfg を直接 INCLUDE。**スタブの logtask 欠落問題を回避**し、TECS/非TECS 両対応を残せる。
- **案B**: `INCLUDE("tecsgen.cfg")` を残し、generated/tecsgen.cfg スタブに logtask を足す（CMakeLists.txt:92-97 を改変）。→ asp3_core 本体改変が必要で M3 スコープ外。**非採用**。

→ **採用は案A**。test_safeg.cfg の `:4` 1行を sample1.cfg 型 4行へ書換える（実装は M3 フェーズ。本書では未改変）。

### 2.4 CRE_TSK/CRE_CYC/DEF_EXC の asp3_core cfg 体系への移行

**記法・配線変更は不要**（標準静的API。asp3_core Python cfg が解釈）。確認事項のみ:

- cfg ファイルの読込口: asp3_core は app cfg を `ASP3_CFG_FILES` 末尾に積む（CMakeLists.txt:164-166）。app cfg は `-DASP3_APPCFGNAME`/`-DASP3_APPLDIR` で外部指定可（CMakeLists.txt:102-128。§5）。
- 優先度の整合（test_safeg.h:15-18）: EXC=1/RESTART=2/HI=4/MAIN=5、LogTask=3（logtask.cfg の `LOGTASK_PRIORITY`）、BTASK=`TMAX_TPRI`。**HI(4) < LogTask(3) でなく、HI は LogTask より低優先（数値大）**＝test_safeg.h:17 のコメント通り。logtask.cfg の `LOGTASK_PRIORITY` 既定値が 3 であることを M3 実装時に確認（要確認・未実測）。
- `DEF_EXC(HARDFAULT_EXCNO=3)`: asp3_core `core_kernel.py:23` の `EXCNO_VALID=[2,3,4,5,6,7,12]` に 3 が含まれる＝合法。`CPUEXC1`(=6 UsageFault) も含まれる。**遷移テストの例外登録は asp3_core の Python cfg チェックを通る**。
- `TNFY_HANDLER`/`TA_STA`/`TA_ACT` 等の属性: 標準。差なし。

---

## 3. test_safeg.cdl 廃止に伴う変更点（セル→直接呼出し）

| 観点 | 廃止前（TECS） | 廃止後（非TECS） |
|---|---|---|
| syslog 出力 | C → `tSysLogAdapter` → `tSysLog` セル | C → `syslog()`（vasyslog.c 実体）。**コード無改変** |
| ログ出力タスク | `tLogTask` セル | `logtask.cfg` の `CRE_TSK(LOGTASK)` |
| 起動メッセージ | `tBanner` セル | `banner.cfg` の `ATT_INI(print_banner)` |
| SIO | `tSerialPort`→`tSIOPort`(target.cdl) | `target_serial.cfg`(CFG_INT/CRE_ISR) + `cmsdk_uart.c` |
| ビルド配線 | tecsgen 実行 → `init_tecs.o`/`gen/Makefile.tecsgen` | tecsgen 不要。`asp3_add_syssvc()` がリンク |

**[TST] AI機械可読出力の同一性**: 出力は全て `syslog(LOG_EMERG,…)`（同期低レベル）。LogTask 非同期ではなく低レベル出力経路を使うため（doc/test_safeg.md:25-26）、TECS/非TECS のセル構成差は出力に影響しない。**START/RESULT_ADDR/CP/CHK/MARK/FAIL/SUMMARY/DONE の全行が非TECSでも同一フォーマットで出る**（doc/test_safeg.md §出力フォーマットと一致）。合否判定（DONE 到達 & fail=0）も不変。

---

## 4. NS 側（ns_baremetal / ns_freertos）無改変の確認 + CMSE implib

### 4.1 NS は無改変

- `ns_test_main.c` は `#include "test_gate.h"` のみ（ns_test_main.c:18-19。kernel非依存）。gate を所定順で呼ぶだけ。**非TECS化の影響なし＝無改変**。
- `test_gate.h` は `<stdint.h>` のみ依存（test_gate.h:13）。Secure 側の cfg/cdl 構成と独立。
- ns_freertos も同契約（`main_test.c`）。**無改変**。

### 4.2 CMSE import lib（secure_nsclib.o）の CMake 体系での扱い（★要新規追加）

現状（Makefile 体系）: Secure ビルドが gate veneer から CMSE import lib を出力し、NS Makefile がそれをリンク:
- 現 NS Makefile: `NSCLIB = ../../../FreeRTOS/sample/mps2_an505_gcc/secure_nsclib.o`（ns_baremetal/mps2_an505_gcc/Makefile:9, 26-27）。
- 現テスト運用: Secure テストビルドが `secure_nsclib.o` を test 用 gate の import lib で上書き（doc/test_safeg.md:205-206）。

asp3_core CMake 体系での現状（実測・確定）: **`-mcmse`/`cmse_nonsecure`/`secure_nsclib.o`/`--cmse` は asp3_core 全ツリーに 0 ヒット＝CMSE import lib 生成は完全に不在**。arch.cmake は `TOPPERS_SAFEG_M`/`TOPPERS_ENABLE_TRUSTZONE` の compile def 付与のみ（arch.cmake:36-39）。なお M0 §11.2 のガードは実装済み（`arch/arm_m_gcc/common/arm_m.h:55-56` に `#if defined(TOPPERS_SAFEG_M) && !defined(TOPPERS_ENABLE_TRUSTZONE)` → `#error`）。

→ **M3 で CMake に CMSE import lib 生成を追加する必要がある**（設計のみ。実装は M3 フェーズ）:
1. Secure 実行ファイル（`asp` ターゲット）のコンパイル/リンクに `-mcmse` を付与（`ENABLE_SAFEG_M=ON` 時のみ）。gate（test_gate.c の `cmse_nonsecure_entry`）から veneer を生成させる。
2. リンク時に `-Wl,--cmse-implib,--out-implib=<dir>/secure_nsclib.o` を付与し import lib を出力（出力先を CMake 変数で公開）。
3. NS 側（CMake 化するなら）または NS Makefile が 2 の出力を参照（パス可変化）。**NS の CMake 化は M3 必須ではない**（NS は別ビルドで現 Makefile 流用可。import lib パスのみ asp3_core ビルド出力へ向ける）。

確定（実測）: asp3_core に CMSE/implib は**一切無い**ため、上記 1〜3 は **M3 での新規追加が必須**（流用元なし）。

---

## 5. ボード別 SIO 接続差 と app の外部ビルド方法

### 5.1 SIO 接続差

| ボード | 用途 | SIO ドライバ（非TECS） | cfg 断片 | 状態 |
|---|---|---|---|---|
| mps2-an505 | QEMU・非遷移CI | `cmsdk_uart.c`(+`target_serial.c`) | `target_serial.cfg`(CFG_INT INTNO_SIO / CRE_ISR ISR_SIO: an505:10-11) | asp3_core 済（target.cmake:91-94） |
| RP2350 (Pico2) | 実機・遷移(主) | `rp2350_uart.c`（経由 `chip_serial.c`。`arch/arm_m_gcc/rp2350/chip.cmake:24-26`） | 各 chip serial cfg 相当 | M2 ボード（M0 §10-Q2） |
| imxrt685 (imxrt600) | 実機・遷移(回帰保険) | `imxrt600_usart.c`（経由 `chip_serial.c`。`arch/arm_m_gcc/imxrt600/chip.cmake:28-30`） | 同上 | chip host 有（M0 §7） |

→ **SIO 差は target.cmake/target_serial.cfg レベルで吸収**され、test 側 cfg/ソースは SIO 非依存（test は serial を直呼びせず syslog 経由＝SIO はログ出力経路としてのみ使用）。**ボード差し替えは `-DASP3_TARGET=` の切替のみ**で test 側変更不要。

### 5.2 app（test）を asp3_core ツリー外からビルドする方法

asp3_core CMakeLists.txt は外部 app を受ける口を持つ（CMakeLists.txt:102-128）:
- `-DASP3_APPLDIR=<test/secure の絶対 or ルート相対パス>`（CMakeLists.txt:105-111）
- `-DASP3_APPLNAME=test_safeg` / `-DASP3_APPCFGNAME=test_safeg`（CMakeLists.txt:124-128）→ app cfg = `<APPLDIR>/test_safeg.cfg`
- `-DASP3_EXTRA_APP_C_FILES=".../test_gate.c;.../test_harness.c"`（CMakeLists.txt:113-122。相対はルート基準で解決）
- workspace sibling（M0 §10-Q3）: SafeG-M repo の CMake が `-DASP3CORE_DIR=../asp3_core` を参照し、上記変数で test/ を app として渡す構成。

→ **test/ のソースは asp3_core ツリーに移さず、外部 app として CMake 変数で渡せる**（M0 §9 の極小化方針と整合）。app main（`ASP3_APP_MAIN_SRC=<APPLDIR>/test_safeg.c`: CMakeLists.txt:366）として test_safeg.c、追加 C に test_gate.c/test_harness.c を渡す。include path に test/common/secure を `ASP3_APP_INCLUDE_DIRS` で追加（CMakeLists.txt:129）。

---

## 6. 廃止 / 書換え / 新規 ファイル一覧（ファイル単位）

| ファイル | 区分 | 内容 | M3 改変主体 |
|---|---|---|---|
| `test/secure/test_safeg.cdl` | **廃止** | tecsgen 専用セル群（全行）。参照を断つ | test/（M3 実装フェーズ） |
| `test/secure/test_safeg.cfg` | **書換え（1→4行）** | `:4 INCLUDE("tecsgen.cfg")` を sample1.cfg 型 `#ifndef TOPPERS_OMIT_TECS … #else INCLUDE syslog/banner/serial/logtask.cfg #endif` へ。CRE_TSK/CRE_CYC/DEF_EXC は不変 | test/ |
| `test/secure/test_safeg.{c,h}` | **無改変** | syslog/カーネルAPI のみ。TECS非依存 | — |
| `test/secure/test_gate.c` | **無改変** | gate（cmse_nonsecure_entry）。syslog 直呼び | — |
| `test/common/test_harness.{c,h}` | **無改変** | [TST] 出力。syslog 経由 | — |
| `test/common/test_gate.h` | **無改変** | kernel非依存契約 | — |
| `test/ns_baremetal/**`, `test/ns_freertos/**` | **無改変** | NS 本体。import lib パスのみ後述 | — |
| `test/ns_*/<board>/Makefile` | **変更（パスのみ・任意）** | `NSCLIB` を asp3_core CMake 出力の `secure_nsclib.o` へ向ける（Makefile:9） | test/（任意。M3 実装フェーズ） |
| asp3_core CMake（CMSE implib 生成） | **新規（要追加・asp3_core側）** | `ENABLE_SAFEG_M=ON` 時に `-mcmse`+`--out-implib` を配線（§4.2）。**asp3_core repo 改変＝本タスク対象外。M3 実装で別途** | asp3_core/ |
| asp3_core `core_kernel.py`（BTASK） | **依存（M1/M2）** | `_SAFEG_BTASK` 生成。M3 ではなく M2 で充足されるべき（§0） | asp3_core/（M2） |
| SafeG-M repo CMake（test app 配線） | **新規** | `-DASP3_APPLDIR/APPLNAME/EXTRA_APP_C_FILES` で test/ を外部 app として渡す薄い CMake（§5.2） | SafeG-M/（M3 実装） |

---

## 7. 移行手順（2段構え）

### Stage A — QEMU(an505) 非遷移（Secure 単独・出力枠組み）: M1 後すぐ
1. `test_safeg.cfg:4` を案A（sample1.cfg 型）へ書換え（logtask 含む4 INCLUDE）。
2. `test_safeg.cdl` 参照を断つ（ビルド配線から外す）。
3. SafeG-M repo に薄い CMake を用意し、`-DASP3_TARGET=mps2_an505_gcc -DASP3_APPLDIR=<test/secure> -DASP3_APPLNAME=test_safeg -DASP3_EXTRA_APP_C_FILES=<test_gate.c;test_harness.c> -DENABLE_SAFEG_M=ON`（or `=OFF` の Secure 単独）でビルド。
4. `cmake --build … --target run`（QEMU。target.cmake:104-108）で `[TST] START`〜`DONE` が syslog 出力されることを確認。**遷移(C/D1)は QEMU 限界（SG/INVEP）で green 不可（M0 §10-Q2,Q5）**＝非遷移系（A/B 相当の Secure 内動作・出力フォーマット健全性）の CI に限定。

### Stage B — 実機 遷移(A〜D1): M2（BTASK/launch_ns/CMSE/FP修正(A)）後
5. asp3_core CMake に CMSE implib 生成を追加（§4.2）。`secure_nsclib.o` の出力先を確定。
6. RP2350(主)/imxrt685(回帰) で Secure（`ENABLE_SAFEG_M=ON`）→ NS の順でビルド・書込み。
7. NS Makefile の `NSCLIB` を 5 の出力へ向ける。`EXTRA_CFLAGS=-DTST_ENABLE_A3 -DTST_ENABLE_C -DTST_ENABLE_D1` で全カテゴリ。
8. シリアル取得で `DONE` & `SUMMARY fail=0` を確認（A〜D1 が安全網）。

---

## 8. リスク・難所

| # | リスク | 影響 | 緩和 |
|---|---|---|---|
| R1 | `_SAFEG_BTASK` が asp3_core `core_kernel.py` に未生成（§0） | M3 ビルド不能（test_safeg.c が参照） | **M2 の前提条件として明記**。M3 Stage B は M2 完了をゲートにする |
| R2 | CMSE implib 生成が CMake 未配線（§4.2） | NS リンク不能・遷移テスト不可 | M3 で `-mcmse`+`--out-implib` を新規配線。先に asp3_core 既存有無を確認（調査中） |
| R3 | tecsgen.cfg 自動スタブが logtask 欠落（CMakeLists.txt:92-97） | LogTask 未生成→ログ機能不整合の恐れ | **案A 採用**で回避（test 側 cfg が4 INCLUDE を明示） |
| R4 | FP退避 HardFault（doc/test_safeg.md の根本原因） | 遷移(C/D1) で HardFault | M2 で修正(A)（BTASK の FP 退避スキップ。doc/test_safeg.md:211-249）が載っていること |
| R5 | LogTask 優先度(3) と HI(4) の整合（§2.4） | 横取り順序の前提崩れ | M3 実装時に logtask.cfg の `LOGTASK_PRIORITY` 実値を確認（未実測） |
| R6 | QEMU は遷移 green 不可（SG/INVEP。M0 §10-Q2,Q5） | QEMU で A3/C/D1 検証不能 | QEMU は非遷移CI専用。遷移の正は実機（A〜D1 安全網） |
| R7 | asp3_core 本体改変（CMSE/BTASK）が他作業と競合 | repo 競合 | 本タスクは doc のみ。実装は M2/M3 で順序立てて |

---

## 9. 検証（実機 A〜D1 が安全網）

- **非遷移（QEMU/an505）**: `[TST] START/RESULT_ADDR/CP A1.../CHK/SUMMARY/DONE` の出力フォーマット健全性。Secure 内タスク（MAIN/LogTask/周期）の起動。fail=0。
- **遷移（実機 RP2350 主 / imxrt685 回帰）**: doc/test_safeg.md の実機既定結果（A1/A2/B1-B4 で `total=5 pass=5 fail=0 DONE`: test_safeg.md:94-110）と、全カテゴリ（A3+C+D1）で `total=9 pass=9 fail=0 DONE`（test_safeg.md:243-249）を非TECSビルドで再現することを合格条件とする。
- 合否は **`DONE` 到達 & `SUMMARY fail=0`**（doc/test_safeg.md:39。AI機械可読）。CHK/CP の番号・期待値は非TECS化で不変。

---

## 10. 工数見積（設計のみ。実装は M3 フェーズ）

| 作業 | 規模 | 前提 |
|---|---|---|
| test_safeg.cfg 書換え（案A）+ cdl 廃止 | 0.5d | — |
| SafeG-M repo 薄 CMake（test app 配線・§5.2） | 1d | asp3_core 外部app口（既存） |
| Stage A: QEMU 非遷移ビルド確認 | 1d | M1 完了 |
| CMSE implib 生成 CMake 追加（§4.2） | 1.5d | asp3_core 既存有無に依存（±1d） |
| NS リンク配線（NSCLIB パス可変化） | 0.5d | — |
| Stage B: 実機 A〜D1 検証（RP2350+imxrt685） | 2〜3d | **M2 完了（BTASK/launch_ns/FP修正(A)）必須** |
| 合計 | **約 6.5〜8d** | M1/M2 が前提 |

---

## 11. 未確認事項（明記）

- U1: ~~asp3_core CMake に既存の `-mcmse`/`secure_nsclib.o` 生成があるか~~ → **解決（実測）: 一切無い**（0 ヒット）。§4.2 を新規追加する。
- U2: asp3_core `syssvc/logtask.cfg` の `LOGTASK_PRIORITY` 実値（HI=4 との整合確認用）。未実測。
- U3: ~~RP2350/imxrt600 の非TECS SIO ドライバ実体ファイル名~~ → **解決（実測）: RP2350=`rp2350_uart.c`、imxrt600=`imxrt600_usart.c`（いずれも `chip_serial.c` 経由）**。SIO は test 非依存。
- U4: asp3_core `target/mps2_an505_gcc` に test 用 `target_test.h`/`core_test.h` が既存（an505:41）。SafeG test の `target_test.h`(SafeG 側 mps2_an505_gcc:53 が core_test.h を include)との整合（INTNO_SIO/TNUM_PORT 等の供給元）は M3 実装時に突合要。
