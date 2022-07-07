#include "config.h"
#include <assert.h>
#include <tests/fuzz/libfuzz.h>

#include <brocoin/pubkey.h>
#include <brocoin/chainparams.h>
#include <brocoin/script.h>
#include <common/close_tx.h>
#include <common/setup.h>
#include <common/utils.h>
#include <wire/wire.h>

void init(int *argc, char ***argv)
{
	common_setup("fuzzer");
	chainparams = chainparams_for_network("brocoin");
}

void run(const uint8_t *data, size_t size)
{
	const uint8_t *wire_ptr;
	size_t wire_max, min_size, script_size;
	struct brocoin_outpoint outpoint;
	struct amount_bro funding, to_us, to_them, dust_limit, max;
	const uint8_t *our_script, *their_script, *funding_script;
	struct pubkey *pk1, *pk2;

	/* create_close_tx wants:
	 * - 3 scripts: 3 * N bytes
	 * - 1 txid: 32 bytes
	 * - 1 u32: 4 bytes
	 * - 4 amount_bro: 4 * 8 bytes
	 *
	 * Since both output scripts size are not restricted, we also try
	 * to vary their length.
	 * Therefore, we allocate the entire remaining bytes to scripts.
	 */
	min_size = 8 * 3 + 4 + 32;
	if (size < min_size + 2)
		return;

	script_size = (size - min_size) / 2;
	wire_ptr = data;

	wire_max = 8;
	to_us = fromwire_amount_bro(&wire_ptr, &wire_max);
	assert(wire_ptr);
	wire_max = 8;
	to_them = fromwire_amount_bro(&wire_ptr, &wire_max);
	assert(wire_ptr);
	wire_max = 8;
	dust_limit = fromwire_amount_bro(&wire_ptr, &wire_max);
	/* The funding must be > to_us + to_them (TODO: we could simulate some fees) .. */
	if (!(amount_bro_add(&funding, to_us, to_them)))
		return;
	/* .. And < max_bron as we assert it's not nonsensical! */
	max = AMOUNT_BRO((u32)WALLY_BRONEES_PER_BRON * WALLY_BRON_MAX);
	if (amount_bro_greater(funding, max)) {
		funding = max;
		to_us = amount_bro_div(max, 2);
		to_them = amount_bro_div(max, 2);
	}

	wire_max = 36;
	fromwire_brocoin_outpoint(&wire_ptr, &wire_max, &outpoint);

	our_script = tal_dup_arr(tmpctx, const uint8_t, wire_ptr, script_size, 0);
	their_script = tal_dup_arr(tmpctx, const uint8_t, wire_ptr + script_size,
				   script_size, 0);

	/* We assert it's valid, so we can't throw garbage at the funding script.. */
	pk1 = tal(tmpctx, struct pubkey);
	pk2 = tal(tmpctx, struct pubkey);
	pubkey_from_hexstr("034fede2c619f647fe7c01d40ae22e4c285291ca2ffb47937bbfb7d6e8285a081f",
			   PUBKEY_CMPR_LEN, pk1);
	pubkey_from_hexstr("028dfe31019dd61fa04c76ad065410e5d063ac2949c04c14b214c1b363e517452f",
			   PUBKEY_CMPR_LEN, pk2);
	funding_script = brocoin_redeem_2of2(tmpctx, pk1, pk2);

	create_close_tx(tmpctx, chainparams, our_script,
			their_script, funding_script, &outpoint,
			funding, to_us, to_them, dust_limit);

	clean_tmpctx();
}
