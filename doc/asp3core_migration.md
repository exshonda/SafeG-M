# ASP3 ベースを asp3_core へ変更する検討 — 案1 vs 案2 比較

> **【SUPERSEDED】** 本書は検討段階の比較メモ。結論（**案1採用**）は `doc/M0_design_freeze.md`（M0 設計凍結, 2026-06-17）に正式確定・詳細化された。最新の確定事項は M0 を参照。本書は経緯記録として残置。

最終更新: 2026-06-16

## 背景・前提
- SafeG-M の ASP3 ベースを、現行 `asp3_3.7`(Makefile+TECS) から **`asp3_core`(`/home/honda/TOPPERS/ASP3CORE/asp3_core`)** へ変更する検討。
- 確定した前提:
  - **通常の asp3(asp3_3.7・Makefile+TECS) はサポート不要**（asp3_core 一本化）。
  - **asp3_core はユーザー管理＝改変可能**。
  - asp3_core = Release **3.7.2**（現行ベースと同版）、**CMake + 非TECS**、**mps2_an505 ネイティブ対応**。

## asp3_core の構造（実測）
現行ベース asp3_3.7 と同じ 3.7.2 だが配布が別物：
- ビルド系: **CMake（`CMakeLists.txt`/`arch.cmake`/`target.cmake`）＋ Python 設定（`core_check.py`/`core_kernel.py`/`core_offset.py` 等）**。
- SafeG が現在持つ `core_kernel.trb`・`Makefile.core`・`tecsgen/`(232ファイル) は **asp3_core に存在しない**（非TECS）。
- M系チップ = **imxrt600・rp2350 のみ**（lpc5500/nrf5340/stm32l5xx は無し）。**imxrt600(mimxrt685) は有り**。
- **an505 ネイティブ**: `target/mps2_an505_gcc`(30ファイル, `cmsdk_uart.c`/`mps2_an505.ld`/`target.cmake`)。SafeG 自作の an505（target 25 + chip `arm_m_gcc/mps2_an505` 14 ＝ **39ファイル**）は**重複**。

## must-override の量（実測：SafeG の TZ 改変を asp3_core へ載せた場合）
SafeG の TZ 改変は asp3_3.7 基準で作られており、構造の違う asp3_core には **そのまま当たらず“載せ替え（再派生）”が必要**。
共通層の主対象（asp3_core に同名存在・3-wayマージ要）:

| ファイル | SafeG行 | asp3_core行 | 差分行 | 備考 |
|---|---:|---:|---:|---|
| `arch/arm_m_gcc/common/core_support.S` | 777 | 552 | **263** | deactivate_nonsecure_interrupts ＋ 修正(A)。最大の山場 |
| `arch/arm_m_gcc/common/arm_m.h` | 285 | 237 | 64 | SAU/SCB_NS/NSACR/TT 等 TZ 定義 |
| `arch/arm_m_gcc/common/core_insn.h` | 226 | 177 | 53 | set_msp_ns/is_secure 等 |
| `arch/arm_m_gcc/common/core_kernel_impl.c` | 441 | 392 | 53 | launch_ns/SAU/ITNS 初期化 |
| `arch/arm_m_gcc/common/core_kernel_impl.h` | 838 | 820 | 18 | IPM/IIPM_ENAALL の TZ 版 |
| `core_rename.def` / `core_rename.h` / `core_unrename.h` | — | — | 計24 | launch_ns 等の識別子追加 |
| `arch/arm_m_gcc/imxrt600/chip_kernel.h` / `chip_sil.h` | — | — | 各小 | TMIN_INTPRI / TBITW_IPRI |

注: 差分行は「SafeG の TZ 追加」＋「asp3_3.7↔asp3_core の配布差」の合算（同版なのでカーネル論理はほぼ同一、TZ は概ね自己完結した `#ifdef` ブロック追加）。
SafeG 固有のビルド/コンフィグ（`core_kernel.trb`/`Makefile.core`/`core_kernel.cfg`）は asp3_core 流儀（`*.py`/`*.cmake`/cfg 断片）で**再実装**が要る。

**まとめ（量）**: TZ 載せ替え対象は common ~8 ＋ imxrt600 chip 2 ＝ **約10ファイル**（うち core_support.S が突出）。一方で asp3_core 化により **tecsgen 232 ＋ an505 重複 39 ＋ 非対象チップ/ターゲット**を一掃できる。

---

## 案1: asp3_core 側に取り込む（upstream / `#ifdef TOPPERS_SAFEG_M`）
asp3_core の該当 ~10ファイルに直接 `#ifdef TOPPERS_SAFEG_M` で TZ 対応を入れる。SafeG-M 側は サンプル＋`test/`＋ボード固有差分のみ保持。an505 は asp3_core ネイティブを使用。

**利点**
- TZ 改変が asp3_core に一元化。**overlay も 3-way マージも不要**＝恒久的な追従負担が消える。
- asp3_core が **素 ASP3 と SafeG の両対応**に（`#ifdef` で共存、`ENABLE_SAFEG_M=0/1`）。
- SafeG-M 本体が極小化（ボード/サンプル/test のみ）。an505 重複・tecsgen を保持しない。
- ASP3 更新は asp3_core の git 履歴・通常の開発フローに乗る（別管理のパッチ/overlay 不要）。

**欠点 / コスト**
- **asp3_core を改変する**（=ユーザーの core リポジトリに SafeG 由来コードが入る。ただし `#ifdef` でガードされ素ビルドに無害）。
- 初回の TZ 載せ替え（特に `core_support.S` 263行差）を asp3_core の版に合わせて行う必要。
- asp3_core の素ビルド(CI 等)に SafeG 用の `#ifdef` 分岐が増える。

## 案2: SafeG-M 側に overlay 保持（asp3_core は無改変参照）
現 Step3 の overlay 方式（`ASP3_BASE`＋探索順 SafeG 優先）を asp3_core 向けに再適用。SafeG は TZ ~10ファイルを実体所有し asp3_core を上書き。

**利点**
- **asp3_core を汚さない**（読み取り専用参照）。core は完全に素のまま。
- Step3 の overlay 機構・`import_*.sh`・3-way 追従フローを流用できる。
- 素 ASP3 と SafeG の境界が明確（SafeG 改変は SafeG-M 側に隔離）。

**欠点 / コスト**
- **must-override ~10ファイルを SafeG-M 側で恒久保守**。asp3_core 更新のたびに 3-way マージ（特に core_support.S）。
- asp3_core は構造が asp3_3.7 と違う（CMake/非TECS/`*.py`）ため、現 overlay 配線（configure.rb/Makefile 系）を **CMake 流儀へ作り直し**が必要＝Step3 配線の再実装。
- overlay の探索順（SafeG→BASE）を asp3_core の CMake ビルドに噛ませる必要。`core_kernel.trb`/`Makefile.core` 相当が core に無いので、結局 asp3_core 側の `*.py`/cmake を SafeG 用に差し替える/拡張する必要が出やすい（＝部分的に core 改変に滲む）。

---

## 比較表

| 観点 | 案1（asp3_core へ取り込み） | 案2（SafeG-M 側 overlay） |
|---|---|---|
| asp3_core の改変 | **あり**（`#ifdef` ガード, 素ビルド無害） | なし（無改変参照） |
| TZ 載せ替え（初回 ~10ファイル, core_support.S 263行） | 要（asp3_core に直接） | 要（SafeG-M 側に, 内容は同等） |
| 恒久的な追従負担 | **小**（core の通常更新に同居, マージ不要） | 中〜大（更新毎に 3-way マージ） |
| ビルド系の作り直し | CMake/`*.py` に SafeG 分岐追加（core 内で完結） | overlay 配線を CMake 流儀へ再実装（境界が複雑） |
| tecsgen 232 / an505 重複 39 | どちらの案でも一掃可（非TECS・core ネイティブ an505 採用） | 同左 |
| SafeG-M 本体の大きさ | **最小**（sample/test/board のみ） | sample/test/board ＋ TZ ~10 ＋ overlay 配線 |
| 素ASP3とSafeGの分離 | 同居（`#ifdef` で区別） | 明確に分離 |
| 初回工数 | 中（TZ載せ替え＋core build分岐） | 中〜大（TZ載せ替え＋overlay配線をCMake化） |
| ASP3更新の運用 | core の git フローに同居 | import + 3-way（Step3同様） |
| 共通のコスト | test/ ハーネスの**非TECS化**（現 tecsgen 依存）, mimxrt685 実機 A〜D1 で回帰確認, QEMU INVEP 限界は不変 | 同左 |

## 推奨
**案1（asp3_core へ #ifdef で取り込み）を推奨。**

根拠:
1. TZ 載せ替え（~10ファイル, core_support.S が山場）は **案1・案2 どちらでも必要**。差は「その後の恒久負担」で、案1は overlay/3-wayマージが不要。
2. asp3_core が **改変可**かつ **通常 asp3 サポート不要** という前提下では、`#ifdef` 同居が最も自然で一元管理できる（asp3_core が素＋SafeG 両対応に）。
3. 案2 は asp3_core の構造差（CMake/非TECS/`*.py`、`trb`/`Makefile.core` 不在）により overlay 配線の再実装が重く、結局 core 側に手が滲みやすい＝案1の利点を取りにくい。
4. 副次的に tecsgen 232・an505 重複 39 を一掃、ビルドは CMake/非TECS に一本化。

**段階案（推奨手順）**: ① asp3_core で素ビルド＋mimxrt685(imxrt600) を確認 → ② TZ ~10ファイルを `#ifdef TOPPERS_SAFEG_M` で asp3_core へ載せ替え（core_support.S 優先）→ ③ an505 は core ネイティブ＋TZ差分、imxrt600 は SafeG ボード差分 → ④ `test/` を非TECS化して CMake ビルドに載せ替え → ⑤ 実機 mimxrt685 で A〜D1 回帰確認（QEMU は INVEP 限界のためビルド/非遷移系）。

## 留意（両案共通）
- **test/ ハーネスの非TECS化**が前提（現 `test/secure/test_safeg.cfg` は `INCLUDE("tecsgen.cfg")`）。asp3_core は非TECS syslog/serial/banner（`cmsdk_uart.c`/`target_serial.c`）を持つので接続先はある。
- mimxrt685 実機が遷移テストの正（QEMU mps2-an505 は SG/INVEP のエミュ限界で A〜D1 完走不可, `doc/qemu_an505_port.md` §13）。
- 非対象チップ/ターゲット（lpc5500/nrf5340/stm32, asp3_core に無い）を SafeG が引き続き要るかは別途判断（要れば SafeG-M 側保持）。
