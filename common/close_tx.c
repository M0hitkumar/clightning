#include "config.h"
#include <assert.h>
#include <brocoin/script.h>
#include <common/close_tx.h>
#include <common/permute_tx.h>
#include <common/psbt_keypath.h>
#include <common/utils.h>

struct brocoin_tx *create_close_tx(const tal_t *ctx,
				   const struct chainparams *chainparams,
				   u32 *local_wallet_index,
				   const struct ext_key *local_wallet_ext_key,
				   const u8 *our_script,
				   const u8 *their_script,
				   const u8 *funding_wscript,
				   const struct brocoin_outpoint *funding,
				   struct amount_bro funding_bros,
				   struct amount_bro to_us,
				   struct amount_bro to_them,
				   struct amount_bro dust_limit)
{
	struct brocoin_tx *tx;
	size_t num_outputs = 0;
	struct amount_bro total_out;
	u8 *script;

	assert(amount_bro_add(&total_out, to_us, to_them));
	assert(amount_bro_less_eq(total_out, funding_bros));

	/* BOLT #3:
	 *
	 * ## Closing Transaction
	 *
	 * Note that there are two possible variants for each node.
	 *
	 * * version: 2
	 * * locktime: 0
	 * * txin count: 1
	 */
	/* Now create close tx: one input, two outputs. */
	tx = brocoin_tx(ctx, chainparams, 1, 2, 0);

	/* Our input spends the anchor tx output. */
	brocoin_tx_add_input(tx, funding,
			     BROCOIN_TX_DEFAULT_SEQUENCE, NULL,
			     funding_bros, NULL, funding_wscript);

	if (amount_bro_greater_eq(to_us, dust_limit)) {
		script = tal_dup_talarr(tx, u8, our_script);
		/* One output is to us. */
		brocoin_tx_add_output(tx, script, NULL, to_us);
		assert((local_wallet_index == NULL) == (local_wallet_ext_key == NULL));
		if (local_wallet_index)
			psbt_add_keypath_to_last_output(
				tx, *local_wallet_index, local_wallet_ext_key);
		num_outputs++;
	}

	if (amount_bro_greater_eq(to_them, dust_limit)) {
		script = tal_dup_talarr(tx, u8, their_script);
		/* Other output is to them. */
		brocoin_tx_add_output(tx, script, NULL, to_them);
		num_outputs++;
	}

	/* Can't have no outputs at all! */
	if (num_outputs == 0)
		return tal_free(tx);

	permute_outputs(tx, NULL, NULL);

	brocoin_tx_finalize(tx);
	assert(brocoin_tx_check(tx));
	return tx;
}
