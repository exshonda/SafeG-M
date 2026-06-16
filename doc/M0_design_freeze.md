# M0 — 設計確定（SafeG-M → asp3_core 載せ替え, 案1）

最終更新: 2026-06-17
前提合意: **repo は分離維持**（SafeG-M / asp3_core は別リポジトリ）／**ソースは asp3_core へ `#ifdef TOPPERS_SAFEG_M` で取り込み（案1）**／**ARM限定・〜半年で次リリース**。RISC-V(Hazard3) 分離は次サイクル。

> M0 はコードを書かない。**載せ替えの設計を凍結する**フェーズ。所要 〜2週。
> 入力: `exshonda/SafeG-M`(asp3_3.7ベース・TZ済) / `exshonda/asp3_core`(CMake・非TECS・3.7.2)。

---

## 1. M0 の成果物（DoD）— 全件確定済
- [x] 差分4分類表の確定（§4・実測）
- [x] `core_support.S` 9サイト棚卸しの確定（§5・実測。swap2/add7）
- [x] ガード/CMakeオプション方針の確定（§6・§11.1-2）
- [x] asp3_core 受け入れ点マッピングとカバレッジギャップの確定（§7）
- [x] DIVERGENCE_MAP 記録様式の確定（§8）
- [x] repo構成・依存方式の確定（§9＝workspace sibling）
- [x] 未決事項 Q1〜Q5 の決着（§10）

M1 以降に**コードを一切書かない**ことを M0 の規律とする。M0 完了＝上記が全て確定し、**M1（QEMU mps2-an505 で素ビルド＋`ENABLE_SAFEG_M=0` Secure単独起動 → `core_support.S` ③pendsv/④svc の #else 載せ替えから着手）** に入れる状態 ＝ **本書をもって M0 凍結**。

---

## 2. スコープ
- 対象: SafeG-M 1.1.0 の TZ 改変を asp3_core(3.7.2) の該当ファイル版へ載せ替える設計。
- 非対象（次サイクル）: RISC-V/Hazard3 分離、EmbedBench/LADDER 本格統合、Zephyr 比較。
- 既存4ボード(LPC55S69 / NUCLEO-L552ZE-Q / i.MX RT685 / nRF5340)のうち、asp3_core に chip host があるのは **imxrt600 のみ**。残りの扱いは §7・§10 で決める。

---

## 3. 訂正事項（前回の誤り）
`core_support.S` の TZ 改変について「9箇所が `#else` 対」と述べたのは誤り。実測の正は次の通り（§5）。
- `#ifdef TOPPERS_SAFEG_M` の**サイト総数 = 9**。
- うち **`#else` 挙動差し替え = 2**（`pendsv_handler` / `svc_handler`、いずれも NS 向け IIPM/basepri 制御）。
- **追加のみ(additive) = 7**（うち6は FPU/例外 save-restore 経路内への挿入、1は末尾の自己完結ルーチン）。
- 結論は不変: **9サイト中8が既存ルーチンに織り込まれ overlay で外出し不可** → 案1（asp3_core へ取り込み）が正。

---

## 4. 差分4分類（実測 seeding・要確定）
SafeG-M/asp3 で `TOPPERS_SAFEG_M` に触れる全29ファイルを4分類。数値は ifdef群の出現数。

### 分類A: 共通アーキ侵襲 — **asp3_core/common に `#ifdef` 載せ替え（案1の本体）**
| ファイル(arch/arm_m_gcc/common) | SAFEG ifdef群 | asp3_core host | 備考 |
|---|---:|:--:|---|
| `core_support.S` | 9 | 有 | 最大の山場。§5 参照 |
| `arm_m.h` | 10 | 有 | SAU/SCB_NS/NSACR/TT 等の定義。**asp3_core 側で CPACR/FPCCR 改変済(CMSIS協調)** との衝突注意 |
| `core_kernel_impl.c` | 9 | 有 | launch_ns / SAU / ITNS 初期化 |
| `core_kernel_impl.h` | 6 | 有 | IPM/IIPM_ENAALL の TZ 版 |
| `core_insn.h` | 2 | 有 | set_msp_ns / is_secure 等 |
| `core_rename.def`/`.h`/`unrename.h` | — | 有 | launch_ns 等の識別子追加 |

### 分類B: ビルド/コンフィグ — **host 不在・asp3_core 流儀へ再実装**
| ファイル | SAFEG ifdef群 | asp3_core host | 対応 |
|---|---:|:--:|---|
| `arch/arm_m_gcc/common/core_kernel.trb` | 2 | **無**(非TECS) | `core_kernel.py`/`*.cmake` 側へ再実装 |
| `arch/arm_m_gcc/common/Makefile.core` | 1 | **無**(CMake) | `arch.cmake` の SafeG 分岐へ |
| `target/*/target_kernel.cfg`(×6) | 各1 | 流儀差 | Python cfg 断片へ移植 |

### 分類C: チップ依存 — **host 有無で扱いが割れる（§7・§10）**
| chip(arch/arm_m_gcc) | chip_kernel.h | chip_sil.h | asp3_core host |
|---|:--:|:--:|:--:|
| `imxrt600` | 2 | 2 | **有** |
| `lpc5500` | 2 | 2 | 無 |
| `mps2_an505` | 2 | 2 | 無(targetはnative有) |
| `nrf5340` | 2 | 2 | 無 |
| `stm32l5xx_stm32cube` | 2 | 2 | 無 |

### 分類D: ボード/サンプル/テスト — **SafeG-M repo 側に保持**
| 対象 | 内容 |
|---|---|
| `target/{lpc55s69evk,nucleo_l552zeq,mimxrt685evk,nrf5340_dk,mps2_an505}_gcc/` | `target_kernel_impl.c`(2〜5) + `.cfg`(1) |
| `sample/sample1.c` | 4 |
| `FreeRTOS/`, `test/`, `tools/` | NS 側・テストハーネス・ツール |

---

## 5. `core_support.S` 9サイト棚卸し（実測・確定）
asp3_core 版 552行 / SafeG 版 777行。9サイトの位置・種別・役割：

| # | 行(SafeG版) | ルーチン | 種別 | 役割 | 移植難度 |
|--:|---|---|---|---|:--:|
| 1 | L162 | (FPU context save) | additive | TZ 時の FPU/Secure stack 退避 | 中 |
| 2 | L217 | (FPU context save) | additive | 同上 | 中 |
| 3 | **L232** | **pendsv_handler** | **#else swap** | `p_runtsk==btask` なら NS 割込許可、否なら `IIPM_ENAALL` | **高** |
| 4 | **L325** | **svc_handler** | **#else swap** | TZ 時 `basepri,r6`／非 TZ 時 `IIPM_ENAALL` | **高** |
| 5 | L349 | (FPU restore) | additive | TZ 時の復帰処理 | 中 |
| 6 | L429 | (FPU restore) | additive | 同上 | 中 |
| 7 | L447 | (FPU restore) | additive | 同上 | 中 |
| 8 | L478 | (exception) | additive | TZ 時の例外処理 | 中 |
| 9 | L606+ | usagefault_handler / deactivate_nest_entry / **deactivate_nonsecure_interrupts** | additive(自己完結) | NS 割込デアクティベートの本体ルーチン群 | 低(独立追加) |

**載せ替え優先順**: ③④（dispatcher の挙動差し替え＝マージ最脆弱）を最初に asp3_core 版 `pendsv_handler`/`svc_handler` へ手当て。次に①②⑤⑥⑦⑧（FPU/例外経路への挿入。asp3_core の FPU save-restore 構造に追従要）。⑨は独立追加なので最後でよい。各サイトは §8 の様式で DIVERGENCE_MAP に `【SAFEG】` タグ記録。

---

## 6. ガード / CMake オプション方針（要確定）
- ソースガードは **既存トークン `TOPPERS_SAFEG_M` を踏襲**（SafeG ツリーで既にこの名前。新機構の発明は不要）。
- CMake 露出: `ENABLE_SAFEG_M`(0/1) → `-DTOPPERS_SAFEG_M`。既定 0（素 ASP3 ビルド無害を保証）。
- **`TOPPERS_ENABLE_TRUSTZONE` との層関係を明文化**（§10-Q1）。asp3_core では既に Secure 単独動作の EXC_RETURN 選択に `TOPPERS_ENABLE_TRUSTZONE` を使用。SafeG-M は「Secure 単独」ではなく「Secure+NS デュアル」なので、`TOPPERS_SAFEG_M ⇒ TOPPERS_ENABLE_TRUSTZONE` の含意関係と EXC_RETURN マトリクスを M0 で表に確定。

---

## 7. asp3_core 受け入れ点マッピング / カバレッジギャップ
- 分類A の common 5ファイル + core_rename系: **全て asp3_core に同名 host 有**（確認済）。→ 案1で直接載せ替え可能。
- 分類B: host 無。`core_kernel.py`/`arch.cmake`/Python cfg へ再実装（Step3 overlay 配線は使わない）。
- 分類C: asp3_core の chip は **imxrt600 / rp2350 のみ**。
  - imxrt600: chip host 有 → SafeG chip 差分を載せ替え。
  - lpc5500 / nrf5340 / stm32l5xx / mps2_an505: chip host 無 → **今サイクル対象外**（§10-Q2 確定。必要なら SafeG-M repo 側 chip 保持で後送り）。
- mps2_an505: asp3_core は **target レベルで native 対応**（`target/mps2_an505_gcc`）。SafeG 自作 an505(target25+chip14=39) は重複。→ native 採用＋TZ差分で一掃。

---

## 8. DIVERGENCE_MAP 記録様式（確定）
asp3_core 既存台帳の列に合わせ、SafeG 由来行を `【SAFEG】` で識別。`core_support.S` の各サイトはソース内コメントにも `/* 【SAFEG】#N */` を付す。

| ファイル / ディレクトリ | 変更種別 | 理由 | 上流変更時のリスク | 担当レイヤ | 最終確認バージョン |
|---|---|---|---|---|---|
| `arch/arm_m_gcc/common/core_support.S` | `#ifdef TOPPERS_SAFEG_M` 9サイト(swap2/add7) | SafeG-M デュアルOS | **pendsv/svc の #else は最脆弱・上流変更時要再確認** | arch【SAFEG】 | 3.7.2 |
| `arch/arm_m_gcc/common/{arm_m.h,core_insn.h,core_kernel_impl.{c,h}}` | `#ifdef` 取り込み | 同上 | arm_m.h は asp3_core CPACR/FPCCR 改変と同居注意 | arch【SAFEG】 | 3.7.2 |
| `arch/arm_m_gcc/common/core_rename.*` | 識別子追加 | launch_ns 等 | — | arch【SAFEG】 | 3.7.2 |
| `arch/arm_m_gcc/imxrt600/chip_*.h` | `#ifdef` 取り込み | TMIN_INTPRI/TBITW_IPRI | — | chip【SAFEG】 | 3.7.2 |

---

## 9. repo 構成・依存方式（要確定）
- SafeG-M repo: 分類D（board/sample/test/FreeRTOS/tools）のみ保持＝**極小化**。an505 重複39・tecsgen は持たない。
- asp3_core repo: 分類A・C(imxrt600) のガード付き TZ を取り込み。
- 依存方式（§10-Q3 ✅確定）: **(b) workspace sibling を採用**（submodule 不採用）。`-DASP3CORE_DIR=../asp3_core` 参照、版は使用 commit を記録して固定。

---

## 10. 未決事項（全件解決済・確定）
- **Q1** ✅ → §11。`TOPPERS_SAFEG_M ⇒ TOPPERS_ENABLE_TRUSTZONE` 必須、EXC_RETURN マトリクス、CMSIS衝突C1の回避を確定。
- **Q2** ✅ ボード集合と順序を確定:
  - 進行順 **QEMU(mps2-an505) → PICO2(RP2350) → 他ボード(SDK扱いは後送り)**。
  - QEMU は §13 の SG/INVEP 限界により**素ビルド + `ENABLE_SAFEG_M=0`(Secure単独)起動 + 非遷移系 CI 専用**（遷移 green は不可）。
  - **遷移(A〜D1)の正は実機**: RP2350(Pico2) を主、imxrt685 を回帰の保険として併走（imxrt600 は唯一の chip host）。
  - lpc5500 / nrf5340 / stm32l5xx は**今サイクル対象外**（必要なら SafeG-M repo 側 chip 保持で後送り判断）。
- **Q3** ✅ **(b) workspace sibling を採用**（submodule 不採用）。
  - asp3_core / SafeG-M を同一ワークスペース直下に横並び。SafeG-M CMake は `-DASP3CORE_DIR=../asp3_core` で参照。両 git は相手を追跡しない。
  - 版固定は submodule SHA の代わりに**使用 asp3_core commit を thin workspace `CLAUDE.md` / CI ログに記録**して再現性担保。
- **Q4** ✅ **非TECS化で確定**（実装は M3、方針は確定）:
  - `test/secure/test_safeg.cfg` の `INCLUDE("tecsgen.cfg")` を除去。残る `CRE_TSK`/`CRE_CYC`/`DEF_EXC` は asp3_core Python cfg へ。
  - `test/secure/test_safeg.cdl`（serial/syslog/banner/logtask セル）を**廃止**。接続先は asp3_core の非TECS syssvc(`syssvc/{serial,logtask,banner,syslog}.c` + `cmsdk_uart.c`(an505)/`rp2350_uart`(pico2))。`tSysLogAdapter`/`tSerialAdapter` は非TECSで消滅（C から `syslog()`/serial 直接呼出し）。
  - 残確認: `test/common/test_harness.c` / `test/secure/test_gate.c` が TECS 生成エントリを呼んでいないか（M3 着手時）。NS 側(`ns_baremetal`/`ns_freertos`)は無改変。
- **Q5** ✅ **QEMU 据え置き・FVP 不採用で確定**。
  - QEMU 機種を替えても SG/INVEP はコアモデル(機種非依存)起因のため改善せず → QEMU は非遷移系 CI のみ。
  - **Arm FVP は不採用**（非OSS・運用制約を避ける）。遷移の正は実機(RP2350/imxrt685)に一本化。
  - CI 最小構成 = **QEMU(非遷移) + 実機(遷移)**。

---

## 11. Q1 解決 — EXC_RETURN マトリクス & TRUSTZONE 含意（確定）

### 11.1 含意関係
- **`TOPPERS_SAFEG_M ⇒ TOPPERS_ENABLE_TRUSTZONE`（必須）**
  - 理由1: SAFEG_M の `core_support.S`(usagefault_handler / deactivate 系)が `EXC_RETURN_S`・`EXC_RETURN_NESTED` を参照。これらは SafeG では `#ifdef TOPPERS_ENABLE_TRUSTZONE` 下でのみ定義。
  - 理由2: SafeG は Secure 側 → `EXC_RETURN=0xfffffffd`(S) が必要。`ENABLE_TRUSTZONE` 未定義かつ ARMv8-M だと `0xffffffbc`(NS) が選ばれ、最初の例外リターンで INVPC UsageFault（asp3_core `rp2350/PORTING.md` の既知症状と同一）。
- **`TOPPERS_ENABLE_TRUSTZONE ⇏ TOPPERS_SAFEG_M`**: Secure 単独(ASP3のみ)は正常構成。asp3_core の rp2350 ポートが実例。

### 11.2 ビルドで強制（M0 で方針確定・M1 で実装）
- `arch.cmake`: `ENABLE_SAFEG_M=1` のとき `ENABLE_TRUSTZONE` を自動 1（または不整合時 fatal error）。
- `arm_m.h` 先頭にガード: `#if defined(TOPPERS_SAFEG_M) && !defined(TOPPERS_ENABLE_TRUSTZONE)` → `#error`。

### 11.3 EXC_RETURN マトリクス（実測デコード）
ビット: ES(bit0=Secure例外) / SPSEL(bit2,1=PSP) / Mode(bit3,1=Thread) / FType(bit4,1=FPフレーム無) / S(bit6,1=Secureレジスタ)

| マクロ | 値 | ES | SP | Mode | S | 用途 | gate | asp3_core |
|---|---|:--:|:--:|:--:|:--:|---|---|:--:|
| `EXC_RETURN`(非TZ) | `0xffffffbc` | 0 | PSP | Thread | NS | NS 復帰 | `!ENABLE_TRUSTZONE` & ARMv8-M | 有 |
| `EXC_RETURN`(TZ) | `0xfffffffd` | 1 | PSP | Thread | S | S 復帰 | `ENABLE_TRUSTZONE` | 有 |
| `EXC_RETURN_NESTED` | `0xFFFFFFF1` | 1 | MSP | Handler | S | deactivate でネスト Handler へ連鎖 | `ENABLE_TRUSTZONE` | **無→要追加** |
| `EXC_RETURN_S`(マスク) | `0x40` | — | — | — | bit6 | usagefault で「S/Thread/PSP」判定(`0x40\|0x8\|0x4=0x4C`) | `ENABLE_TRUSTZONE` | **無→要追加** |

### 11.4 載せ替え時の衝突（要対応・2件）
- **C1（CMSIS衝突の再発リスク・回避必須）**: asp3_core は bare `CPACR`/`FPCCR` マクロを**意図的に削除**し `CPACR_BASE`/`FPCCR_ADDR` を使用（CMSIS/FSP 同居のため。DIVERGENCE_MAP 記載）。SafeG `arm_m.h` は今も bare `CPACR 0xE000ED88` / `FPCCR 0xE000EF34` を定義。→ **載せ替え時に bare 版を持ち込まない**。SafeG コードの `CPACR`/`FPCCR` 参照は `CPACR_BASE`/`FPCCR_ADDR` へ書換え。
- **C2（純追加・命名整合）**: `EXC_RETURN_S`/`EXC_RETURN_NESTED`/`SCB_NS_*`/`SAU_*`/`NSACR`/`NVIC_ITNS0`/`NVIC_NS_IABR0`/`FPCCR_NS`/`TT_RESP_S` は asp3_core に**無く衝突なし＝純追加**。`EXC_RETURN_S`/`_NESTED` は SafeG 同様 `#ifdef TOPPERS_ENABLE_TRUSTZONE` 下へ。`FPCCR_NS` は asp3_core 規則に合わせ `FPCCR_NS_ADDR` を推奨。

### 11.5 RP2350 固有の留意
- bootrom が ARM イメージを Secure で起動 → `ENABLE_TRUSTZONE=1` は元々必須（11.1 理由2と一致）。SAFEG_M 積層時も `EXC_RETURN=0xfffffffd` で整合し、新たな EXC_RETURN 不整合は生じない。
- RP2350 は M33×2。SafeG-M は単核デュアルOS前提 → モニタ実行コアの固定方針を M2 で要決定（M0 スコープ外だが記録）。

---

## 付録: M0 実測コマンド（再現用）
```bash
# 全 SAFEG ファイル列挙
grep -rl 'TOPPERS_SAFEG_M' SafeG-M/asp3 | sort
# core_support.S の 9サイト種別判定(swap/additive)
F=SafeG-M/asp3/arch/arm_m_gcc/common/core_support.S
grep -n '#ifdef TOPPERS_SAFEG_M' "$F" | while read l; do ln=${l%%:*};
  r=$(sed -n "$((ln+1)),$((ln+40))p" "$F" | grep -m1 -E '#else|#endif');
  case "$r" in *else*) echo "L$ln swap";; *) echo "L$ln additive";; esac; done
# asp3_core 側 chip カバレッジ
ls asp3_core/arch/arm_m_gcc/
```
