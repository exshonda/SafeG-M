# RP2350 (Raspberry Pi Pico 2) 実機 bring-up 調査

対象: SafeG-M 遷移テスト(A〜D1) を **RP2350 実機(Pico2)** で回すための bring-up 可否・道筋調査。
位置づけ(M0確定): **遷移(A〜D1)の正は実機。RP2350(Pico2) を主、imxrt685 を回帰の保険**（`M0_design_freeze.md` §10-Q2）。
本書はソース改変・実機書込みをしない**読み取り解析＋環境調査**。実測・未確認を区別して記す。
作成日: 2026-06-17 / 調査機: 本マシン。

> **本調査時点の重大な前提**: RP2350 実機(Pico2)・Pico系デバッグプローブは **現在このマシンに接続されていない**（§3 実測）。書込み・遷移 green 確認には実機+プローブの接続が必須。ツールチェーン・asp3_core ポート・OpenOCD は揃っており、**ソフト側は整っている**。

---

## 0. 結論サマリ

| 観点 | 判定 | 根拠 |
|---|---|---|
| asp3_core rp2350 ポートの完成度 | **高（実機実績あり・本調査でも素ビルド成功）** | §1, PORTING.md |
| 素ビルド再現性 | **OK（実測: clean build 通過）** | §1.4 |
| TZ/SAU/SG が SafeG-M 要求を満たすか | **満たす（bootrom が ARM を Secure 起動・M33 は SG/SAU 完備）** | §2 |
| QEMU で RP2350 | **不可（機種なし）** | §2.3 実測 |
| 実機書込み手段 | **OpenOCD(RPiフォーク)＋CMSIS-DAP で可。本機の openocd は RPiフォーク確認済** | §3 実測 |
| シリアル取得 | **picocom/minicom/cu/screen 全あり。115200/ttyACMx** | §3 実測 |
| 実機/プローブ接続 | **未接続（要調達/接続）** | §3 実測 |
| SafeG-M test の RP2350 移植 | **可。ボード層(target)・NS配置・SIO の追加が必要（chip host は imxrt600 のみ）** | §4 |
| bring-up 工数 | **中**（ASP3 単体は小、SafeG-M TZ 積層＋NS＋test 配線で中） | §5 |

---

## 1. asp3_core rp2350 ポートの状態

### 1.1 構成（実測）
- arch(chip 依存): `arch/arm_m_gcc/rp2350/`
  - `RP2350.h`(自己完結レジスタ定義・pico-sdk 非依存), `chip.cmake`, `chip_kernel*.h`, `chip_sil.h`(`TBITW_IPRI=4`), `chip_serial.{c,h,cfg}`, **`rp2350_uart.{c,h}`(非TECS版 PL011 SIO)**, `chip_syssvc.h`, `chip_stddef.h`, `chip_rename/unrename`, `chip_os_awareness.py`, **`PORTING.md`(21KB・移植記録)**。
- target(ボード依存): `target/pico2_arm_gcc/`（RISC-V版 `pico2_riscv_gcc` と対）
  - `target.cmake`, `chip.cmake`連携, `target_kernel_impl.c`(`hardware_init_hook` 全面実装), `target_timer.{c,h}`(TIMER0 ALARM0 を HRT, 1MHz), `image_def.S`(bootROM 用 IMAGE_DEF 20byte ブロック), `rpi_pico2.ld`(FLASH4MB/RAM512KB), `rpi_pico.h`(`CPU_CLOCK_HZ=150MHz`, `TMAX_INTNO=68`), `presets.json`(preset 名 `pico2_arm`), `run.cmake`(openocd/gdb/swd-debug/console ターゲット), `swd-debug.gdb`, `target_test.h`(ランタイムテスト用)。
- ビルド方式: **CMake + Ninja**（旧 Makefile+configure.rb は廃止と PORTING.md 冒頭注記）。`CMakePresets.json` が `target/pico2_arm_gcc/presets.json` を include。

### 1.2 ビルド方法（実測コマンド）
```bash
cd /home/honda/TOPPERS/ASP3CORE/asp3_core
export PATH="/home/honda/.mcuxpressotools/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi/bin:\
/home/honda/.mcuxpressotools/cmake-3.30.0-linux-x86_64/bin:\
/home/honda/.mcuxpressotools/ninja-1.12.1:$PATH"
cmake --preset pico2_arm -B build/pico2_arm
cmake --build build/pico2_arm        # -> asp.elf
cmake --build build/pico2_arm --target run   # OpenOCD で書込み(実機必要・本調査では未実行)
```

### 1.3 ENABLE_TRUSTZONE の扱い（重要）
- `target/pico2_arm_gcc/target.cmake` の `ASP3_COMPILE_DEFS` に **`TOPPERS_ENABLE_TRUSTZONE` を常時定義**。あわせて `TOPPERS_CORTEX_M33` / `__TARGET_ARCH_THUMB=5` / FPU(`-mfpu=fpv5-sp-d16 -mfloat-abi=softfp`, LAZYSTACKING)。
- これは PORTING.md **★発見1** のとおり「TZ で Secure/NS 分割する」目的ではなく、**bootrom が ARM イメージを Secure ステートで起動するため、EXC_RETURN=0xfffffffd(S) を選ぶための必須スイッチ**。`ENABLE_TRUSTZONE=0` だと最初の例外リターンで **INVPC UsageFault** で即死（`xPSR=0x69000006`/`CFSR` UFSR INVPC）。
- これは `M0_design_freeze.md` §11.1 理由2・§11.5 と完全に一致。**SafeG-M(`TOPPERS_SAFEG_M ⇒ TOPPERS_ENABLE_TRUSTZONE`)を積層しても新たな EXC_RETURN 不整合は生じない**（Secure 起動で 0xfffffffd 整合）。

### 1.4 素ビルド試行結果（実測・本調査で実施）
**clean configure + clean build 成功**（`/tmp/pico2_verify` で実施、実機書込みなし）:
- 使用 gcc: `arm-none-eabi-gcc 14.2.1`（mcuxpresso 同梱）。Python 3.12。
- リンク結果: **`FLASH 22316 B / 4MB (0.53%)`、`RAM 12448 B / 512KB (2.37%)`**。警告でリンク失敗なし。
- 起動可能イメージ検証: `.text` 先頭(0x10000000)に **`_kernel_vector_table`**、IMAGE_DEF マーカ **`d3deffff`(START) / `793512ab`(END)** をフラッシュ先頭付近に確認（PORTING.md ★発見5/7 の objdump 検証手順を再現）。
- 既存の `build/pico2_arm/asp.elf`(284KB, 2026-06-16) も残存。**ポートは現役。**

### 1.5 PORTING.md の既知事項（移植の地雷・実機実績つき）
| # | 事項 | 要点 |
|---|---|---|
| 1 | INVPC | `ENABLE_TRUSTZONE=1` 必須（上記 §1.3） |
| 2 | TIMER0 | RP2350 は独立 TICKS ブロック(0x40108000)設定が必要。未設定だと HRT 無限ループ |
| 3 | PADS ISO | リセット後パッドが ISO 状態。GP0/GP1 の ISO クリア+IE が必要（RP2040 にない手順） |
| 4 | アトミックエイリアス無効 | `sil_orw`(+0x2000) は M33 PPB(CPACR 等)に効かない → `hardware_init_hook` で CPACR を直接 RMW |
| 5 | image_def 強制リンク | `KEEP()` だけではアーカイブ抽出されない → C 側で `image_def_block` をダミー参照 |
| 6 | CLK_*_DIV INT 位置 | RP2040 bit8 → RP2350 bit16 |
| 7 | 配置順 | `.vector` → `.picobin_block` の順（bootROM が先頭 4KB を探索） |
| 8 | TBITW_IPRI | NVIC 優先度 4bit。`chip_sil.h` で `=4`。未定義はビルドエラー |

### 1.6 SIO(UART)・タイマ
- **UART**: RP2350 UART は PL011 互換。非TECS版 `rp2350_uart.{c,h}` + `chip_serial.{c,h,cfg}` あり（`chip.cmake` が `ASP3_SYSSVC_TARGET_C_FILES` に登録）。これは M0 §10-Q4(TECSレス化)・§13 の `rp2350_uart` 言及と整合。
- **タイマ**: TIMER0 ALARM0 を HRT(1MHz, `INTNO_TIMER=16`)。`KERNEL_TIMER=TIM`。SYSTICK 方式は未検証。
- **ランタイムテスト実績**: PORTING.md に asp3 標準ランタイムテスト 36件中 **34 PASS**（cpuexc1/cpuexc4 のみ FAIL=PRIMASK 中 CPU 例外の HardFault エスカレートで arm_m 共通制限・移植固有でない）。

---

## 2. RP2350 で TZ/SG/SAU が SafeG-M 要求を満たすか

### 2.1 ハード要件（SafeG-M が必要とするもの）
SafeG-M の遷移は次に依存（`asp3/arch/arm_m_gcc/common/core_kernel_impl.c launch_ns`、`test/secure/*`）:
- `launch_ns()`: `set_faultmask_ns`/`set_control_ns`/`set_msp_ns`/`SCB_NS_VTOR` 書込み/`FPCCR_NS` クリア/`set_basepri(0)` → `cmse_nonsecure_call` で NS エントリへ分岐（**SG/BXNS 経路**）。
- gate: `cmse_nonsecure_entry`（NS→S の **SG 命令**経由 secure gateway）。
- C: NS 実行中の Secure タイマ割込み横取り（**BASEPRI_S で NS 割込みマスク**）。
- D1: NS の CPU 例外が `AIRCR.BFHFNMINS=0` で **Secure HardFault(exc3)** として入る。

### 2.2 RP2350 の適合性
- **RP2350 = Cortex-M33 ×2、ARMv8-M Mainline + Security Extension(TrustZone-M)** 実装。SG/BXNS/BLXNS・SAU・`*_NS` バンクレジスタ・FPCCR_NS・BASEPRI_S/NS・AIRCR.BFHFNMINS をすべて備える → **SafeG-M の launch_ns/deactivate/Secure gate の要求を満たす**（QEMU の SG/INVEP 限界=コアモデル起因とは無関係に、実機は本物の M33）。
- **bootrom が ARM を Secure 起動** → SafeG-M の「Secure(ASP3)を S、NS をその上で起動」モデルにそのまま乗る（§1.3）。**M0 §11.5 と整合・新たな不整合なし**。
- **SAU**: M0 §4 分類A の `core_kernel_impl.c` の SAU/ITNS 初期化を SafeG-M 積層時に有効化する必要あり。asp3_core 単体(Secure 単独)では SAU 領域分割を使っていない（NS が無いため）。**RP2350 の SAU は標準 ARMv8-M 仕様**なので an505/imxrt685 と同じコードで動く想定（領域アドレスのみボード差）。

### 2.3 QEMU での RP2350（実測）
- 本機 `qemu-system-arm 8.2.2` の `-machine help`: `mps2-an505`/`mps2-an521`(dual M33) はある。**`rp2350` 機種は存在しない**（`raspi*` は Cortex-A の Pi 0/1/2 のみ）。
- → **RP2350 は QEMU で扱えない**。M0 §10-Q2/§13 の「遷移の正は実機」「QEMU は非遷移系 CI 専用(mps2-an505)」と整合。RP2350 の遷移は**実機のみ**で green を取る。

### 2.4 M33×2 — モニタ実行コア固定（M0 §11.5・M2 決定事項）
- asp3_core rp2350 ポートは **Core0 のみ使用、Core1 は bootROM 内で待機**（PORTING.md）。SafeG-M も単核デュアルOS前提 → **モニタ(=Secure ASP3)を Core0 に固定**するのが asp3_core 既定路線と一致し最小コスト。
- 選択肢と影響:
  - **(a) Core0 固定 / Core1 停止(PSM で停止)**: 最も単純。SafeG-M の単核前提と一致。Core1 が NS RAM に触れない保証が自明。**推奨。**
  - **(b) Core1 で NS を別走**: SafeG-M の単核デュアルOSモデルから外れる(SMP化)→**今サイクル対象外**。
  - 影響: Core1 を放置(現状 bootROM 待機)でも遷移テストは Core0 で完結する。省電力化したいなら停止。**決定は M2、本調査の推奨は (a)**。

---

## 3. 実機 bring-up 経路（環境実調査）

### 3.1 接続状況（実測 `lsusb`）— **RP2350/プローブは未接続**
```
1d6b:0002 Linux Foundation 2.0 root hub
17ef:60ee Lenovo TrackPoint Keyboard II
8087:0aa7 Intel Wireless-AC 3168 Bluetooth
0483:374b STMicroelectronics ST-LINK/V2.1     ← STM 用プローブ
1fc9:0090 NXP LPC-LINK2 CMSIS-DAP V5.460       ← LPC/i.MX 用プローブ
1d6b:0003 Linux Foundation 3.0 root hub
```
- **RPi VID `2e8a`(RP2040/RP2350/Debugprobe `2e8a:000c`) は存在しない**。
- 以前 LinkServer に出ていた "Debugprobe on Pico (CMSIS-DAP)" は**今は接続されていない**。Pico2 実機も無い。
- `/dev/ttyACM0`, `/dev/ttyACM1` はあるが、これは **ST-LINK / LPC-LINK2 の VCP**（RP2350 のシリアルではない）。
- → **bring-up を実行するには Pico2 実機＋デバッグプローブ(Debugprobe on Pico 等の CMSIS-DAP)の接続が前提。現状は接続待ち。**

### 3.2 書込みツール（実測）
| ツール | 有無 | 備考 |
|---|---|---|
| **openocd** | **あり `/usr/local/bin/openocd 0.12.0+dev (2026-06-06 build)`** | **RPi フォーク確認**: バイナリ内に `RP2350`/`RP2040`/`"Looking up ROM symbol ... in RP2350 A0 table"`/`"Secure Arm functions"` 文字列。`scripts/target/rp2350.cfg` と `interface/cmsis-dap.cfg` も同梱。**upstream 0.12 は RP2350 非対応だが本機は対応版**（PORTING.md ★OpenOCD注記が要求するフォークを満たす） |
| picotool | **なし** | IMAGE_DEF があれば OpenOCD で ELF 直書きでき不要(PORTING.md)。BOOTSEL/UF2 経路を使うなら別途要 |
| probe-rs | **なし** | 必須でない(OpenOCD で足りる) |
| arm-none-eabi-gdb | **PATH には無いが mcuxpresso に同梱** (`~/.mcuxpressotools/arm-gnu-toolchain-14.2.../bin/arm-none-eabi-gdb`) | `run.cmake` は `arm-none-eabi-gdb` を直呼びするので **PATH 追加が必要**。`gdb-multiarch 15.1` も利用可 |

`target.cmake` の `ASP3_RUN_COMMAND`（書込み）:
```
openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" \
        -c "program <asp.elf> verify reset exit"
```
本機 openocd で**コマンドは成立する**（実行は実機接続後）。

### 3.3 シリアル取得・リセット（実測）
- コンソールツール: **picocom / minicom / cu / screen すべてあり**。`run.cmake` の `console` ターゲットが picocom→minicom→cu の順に自動選択（`TTY=/dev/ttyACM0 BAUD=115200`）。
- RP2350 のシリアルは **Debugprobe 経由で ttyACMx に出る**（PORTING.md 実績: UART0/Debugprobe→ttyACM0, バナー出力確認済）。**接続後に ttyACM 番号を再確認すること**（現状の ttyACM0/1 は別プローブ）。
- リセット: OpenOCD `reset`（`program ... reset` に含む）/ `swd-debug` の `reset init`。
- **udev**: PORTING.md 注記どおり Debugprobe(`2e8a:000c`)の udev ルールが openocd 同梱 `60-openocd.rules` に**無い**可能性 → 接続後にパーミッションエラーが出たら udev ルール追加か sudo。**要実機確認(未確認)**。

### 3.4 ツールチェーン総括（実測）
- `arm-none-eabi-gcc 14.2.1`（mcuxpresso 14.2.rel1。system `/usr/bin/arm-none-eabi-gcc` もあり）, `cmake 3.30`, `ninja 1.12.1`, `python 3.12`。**ビルドに必要なものは全て揃っている**（§1.4 で実証）。

---

## 4. SafeG-M 遷移テスト(test/) を RP2350 で動かすための差分

### 4.1 現状の test/ 構成（実測 `SafeG-M/SafeG-M/test/`）
- `secure/`: `test_safeg.{c,h,cfg}`(A〜D1 本体・タスク/周期/例外), `test_gate.c`(`cmse_nonsecure_entry` gate 群), `test_safeg.cdl`(TECS・**M0 §10-Q4 で廃止確定**)。
- `common/`: `test_gate.h`/`test_harness.{c,h}`（kernel 非依存・**移植不要**）。
- `ns_baremetal/`: `ns_test_main.c`(共通) + **`mps2_an505_gcc/`**(ld+startup), **`mimxrt685evk_gcc/`**(Makefile)。← **RP2350 用は無い**。
- `ns_freertos/`: `mimxrt685evk_gcc/` のみ。
- A〜D1 のビルドフラグ: NS 側 `EXTRA_CFLAGS` に `-DTST_ENABLE_A3 / -DTST_ENABLE_C / -DTST_ENABLE_D1`（既定は A1/A2/B1〜B4）。

### 4.2 imxrt685(現行 green)との比較で必要になる差分
| レイヤ | imxrt685 | RP2350 で必要な作業 |
|---|---|---|
| **Secure chip host** | imxrt600(SafeG-M asp3 にあり) | **SafeG-M asp3 側に rp2350 chip host が無い**。M0 §7 のとおり chip host は imxrt600 のみ。RP2350 を Secure 側で使うには **asp3_core の rp2350 chip(TZ 積層版)を基盤に SafeG-M TZ 差分を載せる**(案1)。SAFEG ifdef を chip に持ち込む必要は薄い(共通部 §4分類A が主) |
| **Secure target(ボード)** | `mimxrt685evk_gcc` | **新規 `pico2_arm_gcc` 相当を Secure 構成で用意**。asp3_core の `target/pico2_arm_gcc` を出発点に、SAU/ITNS 初期化(NS 領域分割)・NS VTOR・`ENABLE_SAFEG_M=1` を配線。`image_def.S`/`rpi_pico2.ld`/`target_timer` は流用可 |
| **リンカ(Secure)** | 既存 | `rpi_pico2.ld` に **NS 領域の予約**(Secure/NS のアドレス分割)を追加。an505 は `0x10200000`(S)↔`0x00200000`(NS)、RAM `0x38200000`↔`0x28200000` のエイリアス分割(`ns_an505.ld` 参照)。RP2350 のメモリ(XIP 0x10000000/SRAM 0x20000000)に合わせ **SAU 領域と NS の ORIGIN を新規設計** |
| **NS 配置/ld/startup** | `ns_baremetal/mimxrt685evk_gcc` | **`ns_baremetal/pico2_arm_gcc/` を新規**: `ns_*.ld`(NS_FLASH/NS_RAM の ORIGIN を RP2350 SAU 分割に合わせる) + `startup_ns.c`(an505 版 `startup_ns.c` を雛形) + Makefile/CMake。`ns_test_main.c` は共通流用 |
| **SIO** | imxrt FlexComm | RP2350 は PL011(`rp2350_uart`)。Secure 側 syslog は asp3_core の非TECS SIO 流用。NS は gate 経由出力なので NS 自前 UART 不要 |
| **gate(CMSE import lib)** | Secure ビルドが `secure_nsclib.o` 生成→NS がリンク | RP2350 でも同手順(`-mcmse` のコールゲート)。**手順は同一、出力物のアドレスがボード依存** |

### 4.3 移植の本質
- **test_safeg.c / test_gate.c / ns_test_main.c / common はボード非依存** → そのまま流用。
- **ボード依存=「Secure target(SAU/NS VTOR/ld)」+「NS 配置(ld/startup)」+「SIO 配線」の 3 点**。これは an505→imxrt685 でやった移植と同型。
- 前提: SafeG-M(案1)の **TZ 差分が asp3_core に取り込まれていること**(M1〜M3)。それが済めば RP2350 ボード層を足すのは an505/imxrt685 と同じ作業量。

### 4.4 注意: NS 文脈での FPU 退避の既知不安定（要確認）
`ns_test_main.c` 冒頭コメント: **A3/C/D1 は NS 文脈切替を伴い、現状の SafeG-M ディスパッチャの FPU コンテキスト退避(`vstmdb {s16-s31}`)で HardFault に至る**(詳細 `doc/test_safeg.md`)。これは imxrt685 で観測された SafeG-M 本体側の課題で、**RP2350 固有ではない**。RP2350 でも同じディスパッチャを使う以上、A3/C/D1 を回す前に**この FPU 退避問題の解消が前提**(M0 §5 の core_support.S FPU サイト①②⑤⑥⑦⑧ 関連)。A1/A2/B1〜B4 は既定で安定。

---

## 5. 工数・難所・推奨ステップ

### 5.1 工数見積り
| フェーズ | 規模 | 理由 |
|---|---|---|
| asp3_core rp2350 単体ビルド/実機起動 | **小** | 既に完成・実機実績・本調査でも素ビルド成功。実機+プローブ接続のみ |
| SafeG-M TZ 差分の asp3_core 取込み(M1〜M3) | **中〜大** | RP2350 固有でなく SafeG-M 全体作業(core_support.S 9サイト等)。RP2350 はその受け皿 |
| RP2350 ボード層(Secure target SAU/ld + NS 配置 + SIO) | **中** | an505/imxrt685 と同型の移植。SAU 領域とメモリマップの新規設計が山 |
| test A1/A2/B 実機 green | **小〜中** | ボード層が出来れば直線。OpenOCD/シリアルは準備済 |
| test A3/C/D1 実機 green | **中** | SafeG-M ディスパッチャの FPU 退避問題(§4.4)解消が前提。RP2350 固有ではない |

### 5.2 難所
1. **実機・プローブが未接続**（最優先・調達/接続）。
2. **RP2350 のメモリ/SAU 領域分割設計**（an505 のエイリアス分割と異なり XIP/SRAM 実体一本。NS 区画の取り方を新規設計）。
3. **NS 文脈の FPU 退避 HardFault**（§4.4。SafeG-M 本体課題、A3/C/D1 の前提）。
4. **udev/プローブ権限**（Debugprobe `2e8a:000c` のルール、未確認）。
5. **PATH 整備**（`arm-none-eabi-gdb` を run.cmake が直呼び）。

### 5.3 推奨ステップ
1. **Pico2 実機＋デバッグプローブを接続**し `lsusb` で `2e8a:*` と CMSIS-DAP、`/dev/ttyACMx`(Pico の VCP)を確認。
2. **asp3_core 単体を実機で起動確認**（`cmake --build build/pico2_arm --target run` + `console`）。本調査で素ビルドは green、あとは書込みのみ。**bring-up 道筋の検証はここで完了する**。
3. SafeG-M(案1) TZ 取込み(M1〜M3)を進める間、**RP2350 を「imxrt600 と並ぶ第2の chip 受け皿」**と位置づける(chip host は現状 imxrt600 のみ。RP2350 は asp3_core 側の rp2350 chip を流用)。
4. SafeG-M ボード層を追加: Secure `target/pico2_arm_gcc`(SAU/NS VTOR/ld) → `test/ns_baremetal/pico2_arm_gcc/`(ld/startup) → SIO 配線。
5. **A1/A2/B から実機 green** → FPU 退避問題解消後に **A3/C/D1**。
6. **imxrt685 を回帰の保険**として併走(imxrt600 は唯一既存の chip host で安定基準線)。**役割分担: RP2350=遷移の主(M0確定)、imxrt685=回帰保険、QEMU mps2-an505=非遷移 CI**。

---

## 6. 未確認・前提事項（明示）
- **RP2350 実機・Pico デバッグプローブは本調査時点で未接続**（§3.1 実測）。書込み・シリアル・遷移 green は接続後に要実施。
- **本調査では実機書込みを一切していない**（指示どおり）。OpenOCD の書込みコマンド成立性のみ確認(RPiフォーク・rp2350.cfg 存在)。
- udev ルール(Debugprobe 権限)・実機での ttyACM 番号は接続後に要確認。
- SafeG-M の RP2350 ボード層・NS 配置・SAU 領域分割は**まだ存在しない**（新規作成対象。本書は道筋のみ）。
- A3/C/D1 の FPU 退避 HardFault は SafeG-M 本体の既知課題（`doc/test_safeg.md`）で、RP2350 固有ではない。

---

## 付録: 本調査の実測コマンド（再現用）
```bash
# 接続確認
lsusb | grep -iE "2e8a|cmsis|debugprobe"   # RP2350/プローブ(現状ヒット無し)
ls /dev/ttyACM*

# openocd が RPi フォーク(RP2350 対応)かの確認
/usr/local/bin/openocd --version
strings /usr/local/bin/openocd | grep -i "RP2350"
ls /usr/local/share/openocd/scripts/target/rp2350.cfg

# asp3_core rp2350 素ビルド(実機書込みなし)
cd /home/honda/TOPPERS/ASP3CORE/asp3_core
export PATH="/home/honda/.mcuxpressotools/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi/bin:\
/home/honda/.mcuxpressotools/cmake-3.30.0-linux-x86_64/bin:\
/home/honda/.mcuxpressotools/ninja-1.12.1:$PATH"
cmake --preset pico2_arm -B /tmp/pico2_verify
cmake --build /tmp/pico2_verify          # FLASH 22316B / RAM 12448B
arm-none-eabi-objdump -s -j .text /tmp/pico2_verify/asp.elf | grep -iE "d3deffff|793512ab"  # IMAGE_DEF
arm-none-eabi-nm -n /tmp/pico2_verify/asp.elf | grep _kernel_vector_table                   # @0x10000000

# QEMU に RP2350 機種が無いことの確認
qemu-system-arm -machine help | grep -iE "rp2350|raspi"
```

---

## 実機 bring-up 確立（2026-06-17, asp3_core vanilla / SAFEG=0）

RP2350(Pico2) 実機＋Pico Debugprobe(CMSIS-DAP `2e8a:000c`) で **asp3_core 単体起動を確認**。Step0 相当の HW パイプライン確立。

### 確認結果
- ターゲット到達: openocd で SWD DPIDR `0x4c013477`、**Cortex-M33 r1p0 ×2(cm0/cm1) 検出・Examination 成功**。
- ビルド: `pico2_arm` preset、FLASH 22144B/4MB・RAM 12448B/512KB（branch `safeg-m-m1`・`ENABLE_SAFEG_M=OFF`＝vanilla 相当）。
- 書込み: `Programming Finished / Verified OK / Resetting Target`、RP2350 rev2・QSPI w25q32 4MB。
- 走行: 停止時 PC=`0x100001c4`（flash内 ASP3 コード＝走行確認）。
- シリアル: **`/dev/ttyACM2` @115200**（Debugprobe VCP）に `TOPPERS/ASP3 Kernel Release 3.7.2 ... / task1 is running (NNN)` を確認。

### 確定コマンド（再現用）
```bash
OOCD=/usr/local/bin/openocd   # RPi フォーク(RP2350対応)
IF="-f interface/cmsis-dap.cfg -c 'transport select swd; adapter speed 5000' -f target/rp2350.cfg"

# 1) ターゲット到達確認(非破壊)
timeout 25 $OOCD -f interface/cmsis-dap.cfg -c "transport select swd; adapter speed 5000" -f target/rp2350.cfg -c "init; targets; exit"

# 2) ビルド
cd /home/honda/TOPPERS/ASP3CORE/asp3_core && cmake --preset pico2_arm && cmake --build --preset pico2_arm

# 3) 書込み+reset（build/pico2_arm/asp.elf）
cd build/pico2_arm && timeout 90 $OOCD -f interface/cmsis-dap.cfg -c "transport select swd; adapter speed 5000" -f target/rp2350.cfg -c "program asp.elf verify reset exit"

# 4) シリアル（Debugprobe VCP=/dev/ttyACM2, 115200）
stty -F /dev/ttyACM2 115200 raw -echo; timeout 10 cat /dev/ttyACM2
```

### 注意 / 申し送り
- **シリアルは /dev/ttyACM2**（asp3_core ttsp の記述と一致）。ttyACM0 は別プローブ(LPC-LINK2/ST-LINK)の VCP で出力なし。
- バナー先頭は捕捉開始タイミングで切れることがある（task1 ループは継続出力）。確実に全体を見るには reset 直後から capture。
- 次段(SafeG TZ 載せ替え後): RP2350 ボード層(Secure target=SAU/NS_VTOR/ld, `test/ns_baremetal/pico2_arm_gcc/`, SIO配線)を an505 同型で新設し、遷移テスト A〜D1 を本実機で。M33×2 のモニタ実行コア固定(Core0)は M2 で決定。

---

## SafeG-M 遷移テスト A〜D1 実機ラン試行（2026-06-17, SafeG=1 / m3-nontecs）

dual-OS 遷移テスト(A1〜D1)のフル green 取得を試行。**ビルドは全工程成功・成果物は実機書込み待ちで準備完了**だが、**RP2350 の reset/halt が timeout する劣化症状**により書込み(program)に至らず中断。物理復旧を依頼。

### ビルド結果（成功・実測）
- **Secure(asp3_core, `pico2_arm` preset, ENABLE_SAFEG_M=ON / ENABLE_SAFEG_IMPLIB=ON)**:
  - app= `test/secure/{test_safeg.c,test_gate.c}` + `test/common/test_harness.c`(同期フラッシュ版 @58e4909)。
  - `FLASH 15968B / SG_veneer 96B / RAM 27816B`。`-mcmse` 有効、`SG_veneer` セクション生成。
  - **`secure_nsclib.o`(556B, gate veneer ×10: tg_checkpoint/chk_u32/mark/get_phase/request_restart/api_check/basepri_check/begin_preempt/end_preempt/finish)生成確認**。`_kernel_launch_ns`@0x10003230, `_SAFEG_BTASK` シンボルあり。
  - ビルド先 `/tmp/run_sec`。
- **NS(test/ns_baremetal/pico2_arm_gcc, `-DTST_ENABLE_A3 -DTST_ENABLE_C -DTST_ENABLE_D1`)**:
  - 上記 `secure_nsclib.o` をリンク。`.text @0x10200000`(NS_VTOR一致), bin 288B。
  - disasm 確認: A3(`tg_get_phase`/`tg_request_restart`)、C(`tg_begin_preempt`/`tg_end_preempt`)、D1(`udf #0`)を含む＝全カテゴリ有効。
- **TST_ENABLE_* は NS 側のみ有効**（`ns_test_main.c` のみが参照。Secure 側は常時全ハンドラ提供）。pass=9 内訳(全 CHK): A2×2(CONTROL/FAULTMASK) + B2(API) + B3×2(BASEPRI entry/after) + C(0xCE end_preempt) + C(hi_task 0xC2/0xC3) + D1(0xD1) = 9。

### 実機書込み（中断・劣化）
- 非破壊 `init; targets`: **成功**（SWD DPIDR `0x4c013477`, cm0/cm1 Examination succeed, CMSIS-DAP 2e8a:000c serial E660583883487B39）。
- `program asp.elf verify` / `reset halt` / `reset init`: **いずれも `Error: timed out while waiting for target halted` / `Unable to reset target`**。
  - 試行: (1) `reset halt`+program 5000kHz → timeout、(2) `program ... verify reset exit` 5000kHz → timeout、(3) `reset init` 2000kHz → timeout。**規定どおり 2 リトライで中断**。
- リセット後シリアル(/dev/ttyACM2): 出力なし(バナー/[TST] とも無し)。
- → **examination は通るが reset/halt 系が一律 timeout**＝doc 規定の劣化兆候。**RP2350 物理復旧(BOOTSEL 起動 or 電源再投入)が必要**。復旧後は上記 `/tmp/run_sec/asp.elf` + `nstest.bin @0x10200000` を `program ... verify reset` で書込み、/dev/ttyACM2 115200 で [TST] A1〜D1 / SUMMARY total=9 を取得する段に直結。
