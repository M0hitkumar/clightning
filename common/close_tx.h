#ifndef LIGHTNING_COMMON_CLOSE_TX_H
#define LIGHTNING_COMMON_CLOSE_TX_H
#include "config.h"
#include <brocoin/tx.h>

struct ext_key;

/* Create close tx to spend the anchor tx output; doesn't fill in
 * input scriptsig. */
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
				   struct amount_bro dust_limit);
#endif /* LIGHTNING_COMMON_CLOSE_TX_H */
