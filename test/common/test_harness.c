/*
 *  SafeG-M 遷移テスト: Secure 側ハーネス実装（AI 向け機械可読出力）
 *
 *  ASP3 標準 test_svc のサービスコール本体を提供する軽量実装．TECS は使わ
 *  ない．出力は syslog(LOG_NOTICE, ...) により "[TST] ..." 形式で 1 行ずつ
 *  行う．出力仕様は doc/test_safeg.md を参照．
 */
#include <kernel.h>
#include <t_syslog.h>
#include "syssvc/syslog.h"
#include "test_harness.h"

/*
 *  デバッガ読み出し用の結果ブロック（シンボル tst_result）
 */
volatile tst_result_t tst_result;

/*
 *  チェックポイント符号(0xA1 等)を "A1" のような文字列へ復号する．
 */
static const char *
decode_cp(uint32_t cp, char *buf)
{
	uint32_t cat = (cp >> 4) & 0xFu;
	uint32_t num = cp & 0xFu;

	if (cat >= 0xA && cat <= 0xD) {
		buf[0] = (char)('A' + (cat - 0xA));
	}
	else {
		buf[0] = '?';
	}
	buf[1] = (char)('0' + num);
	buf[2] = '\0';
	return buf;
}

void
test_start(const char *progname)
{
	tst_result.magic   = TST_MAGIC;
	tst_result.total   = 0;
	tst_result.pass    = 0;
	tst_result.fail    = 0;
	tst_result.last_cp = 0;
	tst_result.done    = 0;
	syslog(LOG_EMERG, "[TST] START prog=%s", (intptr_t)progname);
	syslog(LOG_EMERG, "[TST] RESULT_ADDR 0x%08x", (intptr_t)&tst_result);
}

void
check_point(uint_t count)
{
	/*
	 *  syslog は書式と引数のみを記録し，文字列の整形は後で LogTask が行う．
	 *  そのため %s に渡すバッファはこの関数の寿命を超えて有効でなければ
	 *  ならない（static にする）．
	 */
	static char buf[4];

	tst_result.total++;
	tst_result.last_cp = count;
	(void)decode_cp((uint32_t)count, buf);
	syslog(LOG_EMERG, "[TST] CP %s %d", (intptr_t)buf, (intptr_t)count);
}

void
tst_chk_u32(uint32_t id, uint32_t actual, uint32_t expected)
{
	bool_t ok = (actual == expected);

	if (ok) {
		tst_result.pass++;
	}
	else {
		tst_result.fail++;
	}
	syslog(LOG_EMERG, "[TST] CHK %d exp=0x%08x act=0x%08x %s",
				(intptr_t)id, (intptr_t)expected, (intptr_t)actual,
				(intptr_t)(ok ? "PASS" : "FAIL"));
}

void
tst_mark(uint32_t tag)
{
	syslog(LOG_EMERG, "[TST] MARK 0x%08x", (intptr_t)tag);
}

void
check_assert_error(const char *expr, const char *file, int_t line)
{
	tst_result.fail++;
	syslog(LOG_EMERG, "[TST] FAIL expr=%s file=%s line=%d",
				(intptr_t)expr, (intptr_t)file, (intptr_t)line);
}

void
check_ercd_error(ER ercd, const char *file, int_t line)
{
	tst_result.fail++;
	syslog(LOG_EMERG, "[TST] FAIL ercd=%d file=%s line=%d",
				(intptr_t)ercd, (intptr_t)file, (intptr_t)line);
}

ER
get_interrupt_priority_mask(PRI *p_ipm)
{
	return get_ipm(p_ipm);
}

void
check_finish(uint_t count)
{
	check_point(count);
	tst_done();
}

void
tst_done(void)
{
	tst_result.done = 1;
	syslog(LOG_EMERG, "[TST] SUMMARY total=%d pass=%d fail=%d cp=%d",
				(intptr_t)(tst_result.pass + tst_result.fail),
				(intptr_t)tst_result.pass, (intptr_t)tst_result.fail,
				(intptr_t)tst_result.total);
	syslog(LOG_EMERG, "[TST] DONE");
}
