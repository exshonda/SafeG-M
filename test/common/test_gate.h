/*
 *  SafeG-M 遷移テスト: Secure/Non-secure 共有 gate 契約
 *
 *  このヘッダは Non-secure 側からも include されるため，kernel.h など
 *  Secure 専用ヘッダに依存してはならない（<stdint.h> のみ）．
 *  ここで宣言する関数はすべて Secure 側の cmse_nonsecure_entry gate であり，
 *  Non-secure からは secure_nsclib.o（CMSE import lib）経由で呼び出す．
 */
#ifndef TEST_GATE_H
#define TEST_GATE_H

#include <stdint.h>

/*
 *  チェックポイント番号の符号化
 *    上位ニブル: カテゴリ('A'..'D' を 0xA..0xD で表現)
 *    下位ニブル: 連番
 *  例) 0xA1 -> "A1", 0xB3 -> "B3", 0xD1 -> "D1"
 *  Secure 側ハーネスがこの番号を "A1" のような文字列へ復号して出力する．
 */
#define CP(cat, n)      (((0xA + ((cat) - 'A')) << 4) | (n))

/*
 *  値検査(CHK)の id
 */
#define TGCHK_CONTROL_NS    1u  /* Non-secure 起動時 CONTROL == 0          */
#define TGCHK_FAULTMASK     2u  /* Non-secure 起動時 FAULTMASK == 1        */
#define TGCHK_API_ERCD      3u  /* gate 内 API 発行のエラーコード == E_OK  */
#define TGCHK_BASEPRI_ENTRY 4u  /* gate 入口の BASEPRI_S == 0x80           */
#define TGCHK_BASEPRI_AFTER 5u  /* set_basepri(0) 後の BASEPRI_S == 0      */

/*
 *  Secure 側 gate 群（Non-secure から呼び出し可能）
 *  いずれも内部で必要に応じて check_point/CHK を記録し，末尾で set_basepri(0)
 *  により Non-secure 割込みを再許可してから戻る．
 */
extern void     tg_checkpoint(uint32_t cp);              /* check_point(cp)             */
extern void     tg_chk_u32(uint32_t id, uint32_t actual, uint32_t expected);
extern void     tg_mark(uint32_t tag);                   /* [TST] MARK <tag>            */
extern uint32_t tg_get_phase(void);                      /* 0=初回, 1=再起動後          */
extern void     tg_request_restart(void);               /* A3: BTASK 再起動を要求      */
extern void     tg_api_check(void);                      /* B2: gate 内で API を発行    */
extern void     tg_basepri_check(void);                  /* B3: BASEPRI_S 復帰機構の検査 */
/*
 *  C1/C2/C3: NS が「NS 状態のまま」ループして Secure タイマ割込みによる
 *  横取り・ディスパッチを受けることを検査する．ns_flag は NS の RAM 変数で，
 *  横取りした Secure 側 hi_task が 1 を書き込む（NS はこれを見てループを抜ける）．
 *  ※ ループを Secure gate 内で行うと「NS から呼ばれた Secure ルーチン実行中の
 *    ディスパッチ」となり SafeG-M では不正（pendsv で HardFault）．必ず NS 側で回す．
 */
extern void     tg_begin_preempt(volatile uint32_t *ns_flag); /* C 開始: 横取り待ち登録 */
extern void     tg_end_preempt(void);                    /* C 終了: 横取り成立を判定    */
extern void     tg_puts(const char *s);                  /* A: NS コンソール出力(Secure UART) */
extern void     tg_finish(void);                         /* SUMMARY + DONE を出力       */

#endif /* TEST_GATE_H */
