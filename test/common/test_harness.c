/*
 *  SafeG-M 遷移テスト: Secure 側ハーネス実装（AI 向け機械可読出力）
 *
 *  ASP3 標準 test_svc のサービスコール本体を提供する軽量実装．TECS は使わ
 *  ない．出力は syslog(LOG_EMERG, ...) により "[TST] ..." 形式で 1 行ずつ
 *  行う．出力仕様は doc/test_safeg.md を参照．
 *
 *  ＜同期出力化（LogTask 非依存）＞
 *  ASP3 の syslog は (1) ログバッファへの蓄積（後で LogTask が低レベル出力へ
 *  ドレイン）と (2) lowmask に該当する重要度を即時に低レベル出力（割込み
 *  ロック下で target_fput_log を直接ポーリング）の 2 経路を持つ．既定の
 *  lowmask = LOG_UPTO(LOG_EMERG) なので LOG_EMERG の [TST] 行は既に即時の
 *  低レベル出力経路を通るが，同時にバッファにも積まれ LogTask が後から
 *  再出力するため 1 行が 2 回出てしまう（即時 + LogTask）．
 *
 *  本ハーネスでは test_start() で syslog のログマスクから LOG_EMERG を外し
 *  （lowmask は LOG_UPTO(LOG_EMERG) のまま維持），[TST] 行を「LogTask 用
 *  バッファには積まず，lowmask 即時低レベル出力のみ」とする．これにより
 *    - 出力は LogTask のスケジューリングに依存しない（NS 横取り C / NS 例外
 *      D1 等で LogTask が走れなくても確実に出る）
 *    - 1 行ちょうど 1 回（重複なし），フォーマットは一切不変
 *  となる．即時出力は target_fput_log（an505=cmsdk_uart / rp2350=PL011 等
 *  target 提供の低レベル putc 相当）へ直接書くためボード差は target で吸収．
 *
 *  併せて各 [TST] 行の syslog() 直後に syslog_fls_log()（ASP3 標準サービス
 *  コール syssvc/syslog.h）を呼ぶ．[TST] をバッファに積まない設定下では
 *  これは空ドレインの安全弁（no-op）として働き，万一マスク設定が効かない
 *  環境でもバッファを即時に掃き出して同期性を担保する．
 *  RAM 結果ブロック tst_result は維持．
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
 *  各 [TST] 行を同期出力するためのフラッシュ．
 *  syslog() でログバッファへ積んだ直後にこれを呼ぶことで，LogTask に依存
 *  せず即時に低レベルシリアル出力へドレインする．
 */
#define TST_FLUSH()	(void) syslog_fls_log()

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

	/*
	 *  [TST] 行（LOG_EMERG）を LogTask 用バッファに積まないようログマスクから
	 *  LOG_EMERG を外す（lowmask は LOG_UPTO(LOG_EMERG) のまま即時低レベル
	 *  出力を維持）．以後 [TST] 行は lowmask 即時出力のみで 1 回だけ出る．
	 */
	(void) syslog_msk_log(LOG_UPTO(LOG_NOTICE) & ~LOG_MASK(LOG_EMERG),
						LOG_UPTO(LOG_EMERG));

	syslog(LOG_EMERG, "[TST] START prog=%s", (intptr_t)progname);
	TST_FLUSH();
	syslog(LOG_EMERG, "[TST] RESULT_ADDR 0x%08x", (intptr_t)&tst_result);
	TST_FLUSH();
}

void
check_point(uint_t count)
{
	/*
	 *  [TST] 行は test_start() で LOG_EMERG をログマスクから外しているため
	 *  lowmask 即時パスで syslog() 内に同期整形される（LogTask 遅延整形では
	 *  ない）．よって %s バッファは本関数の寿命だけで足り，static は不要．
	 *  static にすると例外コンテキスト(D1 の CP)とタスクコンテキストの
	 *  check_point が共有 buf を破壊し得るため，ローカルにして再入安全とする．
	 */
	char buf[4];

	tst_result.total++;
	tst_result.last_cp = count;
	(void)decode_cp((uint32_t)count, buf);
	syslog(LOG_EMERG, "[TST] CP %s %d", (intptr_t)buf, (intptr_t)count);
	TST_FLUSH();
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
	TST_FLUSH();
}

void
tst_mark(uint32_t tag)
{
	syslog(LOG_EMERG, "[TST] MARK 0x%08x", (intptr_t)tag);
	TST_FLUSH();
}

/*
 *  Non-secure からの汎用コンソール出力（Secure 経由）。s は Secure 側で
 *  検証済みのローカルバッファを想定する（gate 側で NS ポインタを copy-in する）。
 */
void
tst_puts(const char *s)
{
	syslog(LOG_EMERG, "[NS] %s", (intptr_t)s);
	TST_FLUSH();
}

void
check_assert_error(const char *expr, const char *file, int_t line)
{
	tst_result.fail++;
	syslog(LOG_EMERG, "[TST] FAIL expr=%s file=%s line=%d",
				(intptr_t)expr, (intptr_t)file, (intptr_t)line);
	TST_FLUSH();
}

void
check_ercd_error(ER ercd, const char *file, int_t line)
{
	tst_result.fail++;
	syslog(LOG_EMERG, "[TST] FAIL ercd=%d file=%s line=%d",
				(intptr_t)ercd, (intptr_t)file, (intptr_t)line);
	TST_FLUSH();
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
	TST_FLUSH();
	syslog(LOG_EMERG, "[TST] DONE");
	TST_FLUSH();
}
