# SafeG-M Secure/Non-secure 遷移テスト

対象: i.MX RT685 (mimxrt685evk_gcc)。Secure=ASP3, Non-secure=最小ベアメタル/FreeRTOS。
目的: ASP3 更新(Step2)前後で Secure⇔Non-secure 遷移の回帰を**自動判定**で検出する。
出力は AI(機械)が読みやすい 1 行 1 イベントの安定フォーマット。

## 構成
```
test/
  common/  test_gate.h      … NS/S 共有の gate 契約(kernel非依存)
           test_harness.h   … Secure 側ハーネスAPI(test_svc.h 互換 + AI補助)
           test_harness.c   … ハーネス実装(出力・RAM結果ブロック)
  secure/  test_safeg.{c,h} … Secure 本体(タスク/周期/CPU例外ハンドラ)
           test_gate.c      … cmse_nonsecure_entry の gate 群
           test_safeg.cfg   … CRE_TSK/CRE_CYC/DEF_EXC
           test_safeg.cdl   … syssvc セル(sample1.cdl と同じ)
  ns_baremetal/ ns_test_main.c               … NS 最小テスト本体
                mimxrt685evk_gcc/Makefile    … NS ビルド
  ns_freertos/  …                            … FreeRTOS 版(任意)
```
ハーネスは Secure 側で動くため，NS は **gate 経由**でチェックポイント/値検査を記録する
（gate は sample1.c の `cmse_nonsecure_entry` パターン，末尾 `set_basepri(0)`）。

## 出力フォーマット（AI 向け）
すべて `syslog(LOG_EMERG, ...)` で**同期低レベル出力**する（LogTask 非同期だと
クラッシュ時に消えるため）。プレフィックスは `[TST]`。

| 行 | 意味 |
|----|------|
| `[TST] START prog=<name>` | テスト開始 |
| `[TST] RESULT_ADDR 0x........` | RAM 結果ブロック(`tst_result`)の先頭アドレス |
| `[TST] CP <name> <n>` | チェックポイント通過。`<name>`は符号復号(例 `A1`)，`<n>`は10進(0xA1=161) |
| `[TST] CHK <id> exp=0x.. act=0x.. PASS\|FAIL` | 値検査 |
| `[TST] MARK 0x........` | 任意マーカ |
| `[TST] FAIL expr=.. file=.. line=..` | assert 失敗 |
| `[TST] SUMMARY total=<N> pass=<M> fail=<K> cp=<C>` | 集計 |
| `[TST] DONE` | 終端センチネル（ここまで読めば判定可） |

合否判定: `DONE` が出力され `SUMMARY` の `fail=0` なら PASS。`FAIL`/`CHK ... FAIL` があれば不合格。

### チェックポイント符号化
`CP(cat,n)` = 上位ニブルにカテゴリ('A'..'D'→0xA..0xD)，下位ニブルに連番。
例: A1=0xA1(161), A3=0xA3(163), B1=0xB1(177), B4=0xB4(180), D0=0xD0(208)。

### RAM 結果ブロック（デバッガ読み出し）
シンボル `tst_result`（型 `tst_result_t`，`magic=0x54535400`,`total`,`pass`,`fail`,`last_cp`,`done`）。
起動時に `[TST] RESULT_ADDR` でアドレスを出力（実測 0x30000020 付近, Secure SRAM）。
`LinkServer ... dump <addr> 24 ...` 等で吸い出せる。

## チェックポイント一覧
| ID | カテゴリ | 内容 | 記録箇所 | 既定 |
|----|----------|------|----------|------|
| A1 | Secure→NS 起動 | NS が起動し最初の gate に到達 | NS→tg_checkpoint | ON |
| A2 | 〃 | 起動時 CONTROL_NS==0 / FAULTMASK==1（NSが読みgateで検査） | CHK1,CHK2 | ON |
| A3 | Secure→NS 復帰 | restart_task が BTASK を ter→act し NS が再起動 | NS(phase1) | **opt-in** |
| B1 | NS→Secure gate | gate 呼出しが成立 | NS→tg_checkpoint | ON |
| B2 | 〃 | gate 内 API(get_tid) が E_OK | CHK3 | ON |
| B3 | 〃 | BASEPRI_S を 0x80→0 に復帰できる(BASEPRI_S方式の肝) | CHK4,CHK5 | ON |
| B4 | 〃 | gate の多重呼出し | NS | ON |
| C1 | NS中の割込み | NS実行を Secure タイマで横取り→hi_task 実行 | hi_task | **opt-in** |
| C2 | 〃 | 横取り中 BTASK は READY(TTS_RDY) | hi_task CHK | **opt-in** |
| C3 | 〃 | Secure タスク実行中 BASEPRI_S==0x80(NSマスク) | hi_task CHK | **opt-in** |
| D1 | NS中CPU例外 | NSのudfがHardFault(exc3)としてSecureに上がり hardfault_handler が捕捉(xsns_dpnでNS由来判定) | hardfault_handler(exc3) | **opt-in** |

ビルドフラグ(NS, `EXTRA_CFLAGS`): `-DTST_ENABLE_A3` / `-DTST_ENABLE_C` / `-DTST_ENABLE_D1`。
既定では A1/A2/B1〜B4 のみ（実機 green を確認済み）。

## ビルド & 実行（mimxrt685evk_gcc）
必ず **Secure 先 → NS 後**（NS は Secure が生成する CMSE import lib をリンク）。

```sh
# 1) Secure(ASP3)
cd asp3 && rm -rf build_test && mkdir build_test && cd build_test
ruby ../configure.rb -T mimxrt685evk_gcc \
  -a '$(SRCDIR)/../test/secure $(SRCDIR)/../test/common' \
  -A test_safeg -U 'test_gate.o test_harness.o'
make                      # -> asp.srec, FreeRTOS/sample/mimxrt685evk_gcc/secure_nsclib.o

# 2) Non-secure(ベアメタル)。A3/C/D1 を足すなら EXTRA_CFLAGS を付ける
cd ../../test/ns_baremetal/mimxrt685evk_gcc
make                                   # 既定(A,B)
# make EXTRA_CFLAGS="-DTST_ENABLE_A3 -DTST_ENABLE_C -DTST_ENABLE_D1"   # 全部

# 3) 書込み(LinkServer, プローブ ISA0BQNQ)
LS=/usr/local/LinkServer/LinkServer ; DEV=MIMXRT685S:EVK-MIMXRT685
$LS flash -p ISA0BQNQ $DEV load --erase-all -R asp3/build_test/asp.srec   # Secure
$LS flash -p ISA0BQNQ $DEV load test/ns_baremetal/mimxrt685evk_gcc/gcc/nstest.axf  # NS(reset+run)

# 4) シリアル取得(115200 8N1)。DONE まで読む
stty -F /dev/ttyACM1 115200 raw -echo ; timeout 15 cat /dev/ttyACM1
```

## 実機結果（既定ビルド, 確認済み）
```
[TST] START prog=safeg-trans
[TST] RESULT_ADDR 0x30000020
[TST] CP A1 161
[TST] CHK 1 exp=0x00000000 act=0x00000000 PASS   ← A2 CONTROL_NS==0
[TST] CHK 2 exp=0x00000001 act=0x00000001 PASS   ← A2 FAULTMASK==1
[TST] CP B1 177
[TST] CHK 3 exp=0x00000000 act=0x00000000 PASS   ← B2 API E_OK(ercd)
[TST] CHK 4 exp=0x00000080 act=0x00000080 PASS   ← B3 BASEPRI_S 入口=0x80
[TST] CHK 5 exp=0x00000000 act=0x00000000 PASS   ← B3 set_basepri(0)後=0
[TST] MARK 0x000000b4
[TST] CP B4 180
[TST] MARK 0x000000b4
[TST] SUMMARY total=5 pass=5 fail=0 cp=3
[TST] DONE
```
→ **A1, A2, B1, B2, B3, B4 すべて PASS**。

## 重要な発見（Step2 への申し送り）★
A3 / C / D1 を有効化すると，あるいは既定ビルドでも **DONE 出力後**に，PendSV
ディスパッチャの **FPU コンテキスト退避**で HardFault に至る:

```
Excno = 00000003(HardFault) PC = 0x180026b8 付近
  → _kernel_pendsv_handler 内 `vstmdb r2!, {s16-s31}` (FP レジスタ退避)
basepri = 0x20
```

- 発生条件: **NS(BTASK) を切替対象とする文脈切替**（A3 の `ter_tsk/act_tsk`，C の
  NS 横取り，テスト完了後の周期ディスパッチ）。NS は FPU(遅延スタッキング)コンテキストを
  持ちうるため，その退避時にフォルトする。
- 影響: Secure タイマ等で NS を横取りして高優先度 Secure タスクへディスパッチする
  経路（= SafeG-M の根幹）に，FPU 構成(`FPU_LAZYSTACKING`)依存の不安定性がある可能性。
- 切り分け: 既定ビルド(A,B)は **DONE までは安定して PASS**。クラッシュは DONE 後のため
  AI 判定（DONE/SUMMARY を読む）には影響しない。`tg_finish` で `stp_cyc` しても残存。
- 解析の手掛かり: `doc/analysis_imxrt685.md` 3.2/3.3（BTASK ディスパッチ，
  `core_support.S` pendsv），FPU 退避は EXC_RETURN bit4(FType) と CONTROL.FPCA/FPCCR_NS に依存。
- 次アクション: ① 単純な FPU 無効ビルド(`FPU_USAGE` 変更)で再現有無を確認 ②
  pendsv の FP 退避が NS 文脈で正しいスタック(PSP_S/PSPLIM)を使っているか確認 ③
  これを Step2 の最重要回帰チェック項目とする。

## 根本原因の切り分け結果（2026-06-16 実機調査, 確定）★★
判定: **SafeG-M 本体(FPU_LAZYSTACKING + CMSE gate)の潜在バグ**。テスト不具合ではない。

### 証拠
1. **再現**: NS を `-DTST_ENABLE_A3 -DTST_ENABLE_C -DTST_ENABLE_D1` でビルドし実機実行すると，
   C シナリオ(NS 横取り→hi_task ディスパッチ)の途中(`MARK 0xc0` 直後)で
   `Excno=00000003(HardFault) PC=180026c0 basepri=00000020 p_excinf=30001688`。
   逆アセンブルで **PC=0x180026c0 は `_kernel_pendsv_handler` 内 `vstmdb r2!,{s16-s31}`** と一致
   (`core_support.S:162`)。basepri=0x20 は PendSV 入口の `IIPM_LOCK`。
   p_excinf=0x30001688 は **MSP/istack 範囲(`_kernel_istack`=0x300006b0, size 0x1000 → ..0x300016b0)** 内＝
   HardFault フレームは MSP に積まれた。

2. **FPU 無効化で一発切り分け(決定的)**: `Makefile.target` の `FPU_USAGE = FPU_LAZYSTACKING` を
   `NONE` に変更し Secure/NS を再ビルド・再書込みすると，**`vstmdb` の HardFault は消滅**し，
   **C1/C2/C3 が PASS**(`CHK 194/195 PASS`)，D0 まで到達。
   → FPU コンテキスト退避(`vstmdb {s16-s31}`)が原因であることが確定。
   (注: FPU 無効時は別問題として D1 の NS 例外フォワーディングで別の HardFault
    `PC=0x0 XPSR=0x0` が出るが，これは vstmdb 問題とは別系統＝D1 の今後の課題。)
   (注: `FPU_USAGE=NONE` ビルドには `launch_ns` の `FPCCR_NS` 参照を `#ifdef TOPPERS_FPU_ENABLE`
    で囲う必要があった＝本体に FPU 無効構成が通らない潜在不整合あり。実験後に復元済み。)

3. **FP コンテキストの出所を特定**: NS テストバイナリは **FP 命令を 1 つも実行しない**
   (objdump で vstr/vldr/vmov 等が 0 個, NS Makefile に `-mfpu` 無し)。にもかかわらず
   退避時 `lr.FType(bit4)==0`(FP コンテキスト有り)になる原因は，
   **GCC が `-mfpu`/`-mcmse` で生成する Secure gate veneer (`__acle_se_*`)** にある。
   veneer は NS へ戻る直前に **FP スクラッチ(s0-s15)/FPSCR をゼロ化**する
   (`vmov.f32 s0..s15, #1.0` + FPSCR クリア; 例: `__acle_se_tg_checkpoint` 0x1800136c-0x180013c2)。
   これは Secure 情報の NS 漏洩防止の標準シーケンスで，C コードが float を使わなくても出る。
   → この FP 実行で **`CONTROL_S.FPCA=1`** がセットされる。

### メカニズム(因果連鎖)
1. NS が Secure gate を呼ぶ → veneer が FP レジスタをスクラッチ → BTASK の Secure 文脈で
   `CONTROL_S.FPCA=1`。gate は `bxns` で NS に戻る(BTASK は NS 実行継続)。
2. NS 実行中に Secure タイマ割込み → HW が Secure 例外フレームを MSP に積み，FPCA=1 なので
   **FP 退避領域を予約し `FPCCR_S.LSPACT=1`/`FPCAR_S` 設定，EXC_RETURN.FType=0**。
3. PendSV が BTASK→hi_task のディスパッチを実行。`p_runtsk==BTASK` のため BTASK 文脈を退避:
   `tst lr,#0x10` が FType=0 を見て **`vstmdb r2!,{s16-s31}` を PSP_S(BTASK の Secure スタック)へ**。
4. この `vstmdb` で HardFault。退避先 PSP_S が **PSPLIM_S 違反(スタックオーバーフロー→UsageFault
   →HardFault エスカレーション)**，もしくは lazy s0-s15 push(FPCAR_S 経由)との相互作用で失敗する。
   (CFSR/PSPLIM の実機ライブ読みは本環境の gdbserver が不安定で未取得。サブ機序の最終特定は今後。)

### sample1+FreeRTOS が動く理由
sample の gate も同じ `cmse_nonsecure_entry`＋同コンパイルフラグで **同じ FP スクラッチ veneer を持つ**。
つまり潜在バグは sample でも存在するが，「BTASK の Secure 文脈で FPCA=1 のまま」＋
「その状態で BTASK を別 Secure タスクへ横取りディスパッチ」＋「BTASK Secure スタックの余裕」の
タイミングが揃わないため顕在化しない。test は周期ディスパッチと C 横取りでこの条件を踏む。

### 最小修正案(本体改変, 要相談 — パッチ依存削減方針のため未適用)
本質は「**BTASK(NS) の Secure 文脈に，gate veneer 由来の不要な FP コンテキスト(FPCA/LSPACT)が
残ったまま PendSV が FP 退避を試みる**」こと。候補:
- (A) **BTASK は FP コンテキストを持たない**と扱う: `pendsv_handler` の FP 退避(と
  `pendsv_handler_1`/`svc_handler`/`dispatcher` の FP 復帰)で，対象が BTASK
  (`p_runtsk==_kernel_tcb_table`)のときは `lr.FType` に依らず FP 退避/復帰をスキップする。
  既に BASEPRI 切替で BTASK 判定はしているので分岐追加は容易。
- (B) gate veneer 復帰後/`launch_ns` で **`CONTROL_S.FPCA` をクリア**し，BTASK 文脈の
  FP-active を持ち越さない(`launch_ns` は既に `FPCCR_NS` をクリアしているが，
  `FPCCR_S.LSPACT`/`CONTROL_S.FPCA` は未処理)。
- (C) BTASK の Secure スタック(`DEFAULT_ISTKSZ`=4KB)に FP 退避分の余裕を確保
  (対症療法。PSPLIM 違反が主因なら有効だが本質解ではない)。
推奨は (A)。次段で (A) を実装し A3/C/D1 の PASS を実機確認すること。

### Step2 への申し送り(更新)
- 回帰チェックは **C(NS 横取りディスパッチ)を必ず含める**。既定(A,B)ビルドは DONE まで
  到達するため緑に見えるが，FP 退避バグは隠れる。
- 上記修正(A)を入れた後，`FPU_USAGE=FPU_NO_LAZYSTACKING` でも別途確認すると lazy 依存か
  退避先(PSP_S/PSPLIM)依存かを更に切り分けられる。

## 既知の運用上の注意
- `--erase-all -R`(Secure) 後に NS を `load`(reset) する順で書く。NS は Secure の
  `secure_nsclib.o` をリンクするため Secure を作り直したら NS も作り直す。
- Secure テストビルドは `FreeRTOS/sample/mimxrt685evk_gcc/secure_nsclib.o` を
  test 用 gate の import lib で**上書きする**（FreeRTOS sample を作り直す際は再生成要）。
- シリアルは古いバッファを掃くため取得前に `timeout 2 cat` で drain すると安定。

---

## 修正(A) 適用結果（BTASK の FPU コンテキスト退避スキップ）

確定した根本原因（gate veneer の FP スクラッチで BTASK 文脈に CONTROL_S.FPCA=1 が残り，
BTASK 横取りディスパッチ時の `vstmdb {s16-s31}` が PSPLIM_S 違反 HardFault）に対し，
**修正案(A)** を `asp3/arch/arm_m_gcc/common/core_support.S` に適用した（`TOPPERS_SAFEG_M`
かつ FPU 有効時のみ有効。BTASK= `p_runtsk==_kernel_tcb_table` の判定で FP 退避/復帰を対称にスキップ）。

変更箇所（4 サイト, いずれも `#ifdef TOPPERS_SAFEG_M` ガード）:
- `pendsv_handler` 退避 (`core_support.S:~164`): BTASK なら `vstmdb {s16-s31}` をスキップ（=元の HardFault 発生点）
- `pendsv_handler` 復帰 (`:~218`): BTASK なら `vldmia {s16-s31}` をスキップ
- `svc_handler` 復帰 (`:~339`): ldmfd で r4 が壊れる前に BTASK 判定(r3)，BTASK なら `vldmia` スキップ
- `do_dispatch` 退避 (`:~431`, `dispatch_btask_nofp`): BTASK なら `vpush` せず FPCA クリア＋`fpu_flag=0`（→`return_to_thread` の `vpop` も自然にスキップ）

### 実機検証結果（mimxrt685evk_gcc, ベアメタル NS）
- A3+C 有効(D1 無効): `SUMMARY total=8 pass=8 fail=0` → `DONE`。**A1/A2/A3/B1-B4/C1/C2/C3 すべて PASS**，`vstmdb` HardFault は発生せず。複数周回しても安定。
- 既定(A,B)build: `SUMMARY total=5 pass=5 fail=0` → `DONE`，22 秒連続で `Excno` 出力 0 件（**DONE 後クラッシュの原疾患は解消**）。
- FreeRTOS NS 版: `total=5 pass=5 fail=0` → `DONE`，`Excno` 0 件（非回帰）。注: FreeRTOS 版 main_test.c は A/B 系列(5 CP)のみで A3/C の CP を含まないため flag 有無で件数は変わらない。

### D1（NS 中の CPU 例外捕捉）の作り替え・解決（確定）
当初 D1 を有効化すると `udf #0`(NS) が **Secure HardFault(Excno=3, PC=0)** として上がり「Unregistered
Exception」になっていた（修正前の `vstmdb`(PC=0x180026c0) とは別物＝FPU 修正とは無関係）。原因は，
NS の UsageFault が NS 側未許可のため **HardFault へエスカレーション→Secure 側(既定 BFHFNMINS=0)へ**
到達するが，テストが登録するのは `DEF_EXC(CPUEXC1=6, ...)`(UsageFault) のみで **exc3 ハンドラが無かった**こと。
加えて SafeG-M は Secure UsageFault(exc6) を `deactivate_nonsecure_interrupts` 用に専有しているため，
NS 由来 CPU 例外の Secure 捕捉は exc6 では受けられない。

**作り替え（test/ 側のみで完結。asp3 本体は未改変）**:
- `test/secure/test_safeg.h:29` `#define HARDFAULT_EXCNO 3`，`:51` `hardfault_handler` プロトタイプ。
- `test/secure/test_safeg.cfg:24` `DEF_EXC(HARDFAULT_EXCNO, { TA_NULL, hardfault_handler });`（exc6 の cpuexc_handler 登録は維持）。
- `test/secure/test_safeg.c:147` `hardfault_handler(void *p_excinf)`：`xsns_dpn(p_excinf)` で NS 由来を判定し D1 の CP を記録，`SCB_HFSR` を W1C してテスト継続。

**実機検証結果（mimxrt685evk_gcc, ベアメタル NS, D1+A3+C 有効）**:
```
[TST] CP A1 161 / CHK1,CHK2 PASS / CP A3 163 / CP B1 177 / CHK3,4,5 PASS / CP B4 180
[TST] CP C1 193 / CHK194,CHK195 PASS / CHK206 PASS / CP D0 208 / CP D1 209 / CHK209 PASS
[TST] SUMMARY total=9 pass=9 fail=0 cp=7
[TST] DONE
```
→ **D1 PASS。A1/A2/A3/B1-B4/C1-C3/D1 全 9 件 PASS，fail=0，DONE 到達。Excno=3 Unregistered 停止は消滅**（fault 行 0 件）。A/B/C も非回帰。

注（軽微・表示上）: 1 ブート内でテストが複数回ループ実行され，2 周目以降は CP の表示名が前周の最後の名前（例 "B4"）のまま番号だけ更新される箇所がある（CHK 値・番号・SUMMARY は正しく，合否判定には無影響）。harness の CP 名更新を CP ごとに行うようにすれば解消できる将来課題。
