/*
 *  SafeG-M 遷移テスト: Secure 側ハーネス API（AI 向け機械可読出力）
 *
 *  ASP3 標準 test_svc.h の check_point / check_assert / check_ercd /
 *  check_state / check_ipm をそのまま利用できるよう同ヘッダを取り込み，
 *  実装(test_harness.c)で各サービスコールの本体を提供する．
 *  出力はすべて syslog 経由で 1 行ずつ "[TST] ..." 形式で行う（仕様は
 *  doc/test_safeg.md 参照）．
 */
#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <kernel.h>
#include <t_syslog.h>
#include "syssvc/test_svc.h"  /* check_point/check_assert 等の宣言とマクロ */
#include "test_gate.h"    /* チェックポイント符号化 CP(), CHK id        */

/*
 *  AI 向け補助 API
 */
extern void tst_chk_u32(uint32_t id, uint32_t actual, uint32_t expected);
extern void tst_mark(uint32_t tag);
extern void tst_puts(const char *s);  /* [NS] <s> を出力(NS コンソール用) */
extern void tst_done(void);     /* SUMMARY 行 + DONE センチネルを出力 */

/*
 *  デバッガ(LinkServer/gdb)からも読めるよう結果を保持する固定構造体．
 *  シンボル名 tst_result で参照可能（アドレスは起動時に [TST] RESULT_ADDR で出力）．
 */
typedef struct {
	uint32_t	magic;      /* 0x54535400 ("TST\0") で初期化済みを判別 */
	uint32_t	total;      /* check_point 通過数                       */
	uint32_t	pass;       /* CHK/assert の成功数                      */
	uint32_t	fail;       /* CHK/assert/ercd の失敗数                 */
	uint32_t	last_cp;    /* 最後に通過した check_point 番号(符号化)  */
	uint32_t	done;       /* tst_done 到達で 1                        */
} tst_result_t;

extern volatile tst_result_t tst_result;

#define TST_MAGIC	0x54535400u

#endif /* TEST_HARNESS_H */
