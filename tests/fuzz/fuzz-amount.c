#include "config.h"
#include <assert.h>
#include <tests/fuzz/libfuzz.h>

#include <common/amount.h>

void init(int *argc, char ***argv)
{
}

void run(const uint8_t *data, size_t size)
{
	struct amount_mbro mbro;
	struct amount_bro bro;
	char *string;
	uint8_t *buf;
	const char *fmt_mbro, *fmt_mbrobron, *fmt_bro, *fmt_brobron;


	/* We should not crash when parsing any string. */

	string = to_string(NULL, data, size);
	parse_amount_mbro(&mbro, string, tal_count(string));
	parse_amount_bro(&bro, string, tal_count(string));
	tal_free(string);


	/* Same with the wire primitives. */

	buf = tal_arr(NULL, uint8_t, 8);

	mbro = fromwire_amount_mbro(&data, &size);
	towire_amount_mbro(&buf, mbro);
	bro = fromwire_amount_bro(&data, &size);
	towire_amount_bro(&buf, bro);

	tal_free(buf);


	/* Format should inconditionally produce valid amount strings according to our
	 * parser */

	fmt_mbro = fmt_amount_mbro(NULL, mbro);
	fmt_mbrobron = fmt_amount_mbro_bron(NULL, mbro, true);
	assert(parse_amount_mbro(&mbro, fmt_mbro, tal_count(fmt_mbro)));
	assert(parse_amount_mbro(&mbro, fmt_mbrobron, tal_count(fmt_mbrobron)));
	tal_free(fmt_mbro);
	tal_free(fmt_mbrobron);

	fmt_bro = fmt_amount_bro(NULL, bro);
	fmt_brobron = fmt_amount_bro_bron(NULL, bro, true);
	assert(parse_amount_bro(&bro, fmt_bro, tal_count(fmt_bro)));
	assert(parse_amount_bro(&bro, fmt_brobron, tal_count(fmt_brobron)));
	tal_free(fmt_bro);
	tal_free(fmt_brobron);
}
