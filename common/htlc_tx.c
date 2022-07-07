#include "config.h"
#include <brocoin/preimage.h>
#include <brocoin/script.h>
#include <common/htlc_tx.h>
#include <common/keyset.h>

static struct brocoin_tx *htlc_tx(const tal_t *ctx,
				  const struct chainparams *chainparams,
				  const struct brocoin_outpoint *commit,
				  const u8 *commit_wscript,
				  struct amount_mbro mbro,
				  u16 to_self_delay,
				  const struct pubkey *revocation_pubkey,
				  const struct pubkey *local_delayedkey,
				  struct amount_bro htlc_fee,
				  u32 locktime,
				  bool option_anchor_outputs)
{
	/* BOLT #3:
	 * * locktime: `0` for HTLC-success, `cltv_expiry` for HTLC-timeout
	 */
	struct brocoin_tx *tx = brocoin_tx(ctx, chainparams, 1, 1, locktime);
	u8 *wscript;
	struct amount_bro amount;

	/* BOLT #3:
	 *
	 * ## HTLC-Timeout and HTLC-Success Transactions
	 *
	 * These HTLC transactions are almost identical, except the
	 * HTLC-timeout transaction is timelocked.  Both
	 * HTLC-timeout/HTLC-success transactions can be spent by a valid
	 * penalty transaction.
	 */

	/* BOLT #3:
	 * * version: 2
	 */
	assert(tx->wtx->version == 2);

	/* BOLT #3:
	 * * txin count: 1
	 *    * `txin[0]` outpoint: `txid` of the commitment transaction and
	 *      `output_index` of the matching HTLC output for the HTLC
	 *      transaction
	 *    * `txin[0]` sequence: `0` (set to `1` for `option_anchors`)
	 */
	amount = amount_mbro_to_bro_round_down(mbro);
	brocoin_tx_add_input(tx, commit,
			     option_anchor_outputs ? 1 : 0,
			     NULL, amount, NULL, commit_wscript);

	/* BOLT #3:
	 * * txout count: 1
	 *    * `txout[0]` amount: the HTLC amount minus fees
	 *       (see [Fee Calculation](#fee-calculation))
	 *    * `txout[0]` script: version-0 P2WSH with witness script as shown
	 *       below
	 */
	if (!amount_bro_sub(&amount, amount, htlc_fee))
		abort();

	wscript = brocoin_wscript_htlc_tx(tx, to_self_delay, revocation_pubkey,
					  local_delayedkey);
	brocoin_tx_add_output(tx, scriptpubkey_p2wsh(tmpctx, wscript),
			      wscript, amount);

	brocoin_tx_finalize(tx);
	assert(brocoin_tx_check(tx));

	tal_free(wscript);

	return tx;
}

struct brocoin_tx *htlc_success_tx(const tal_t *ctx,
				   const struct chainparams *chainparams,
				   const struct brocoin_outpoint *commit,
				   const u8 *commit_wscript,
				   struct amount_mbro htlc_mbronees,
				   u16 to_self_delay,
				   u32 feerate_per_kw,
				   const struct keyset *keyset,
				   bool option_anchor_outputs)
{
	/* BOLT #3:
	 * * locktime: `0` for HTLC-success, `cltv_expiry` for HTLC-timeout
	 */
	return htlc_tx(ctx, chainparams, commit,
		       commit_wscript, htlc_mbronees,
		       to_self_delay,
		       &keyset->self_revocation_key,
		       &keyset->self_delayed_payment_key,
		       htlc_success_fee(feerate_per_kw,
					option_anchor_outputs),
		       0,
		       option_anchor_outputs);
}

/* Fill in the witness for HTLC-success tx produced above. */
void htlc_success_tx_add_witness(struct brocoin_tx *htlc_success,
				 const struct abs_locktime *htlc_abstimeout,
				 const struct pubkey *localhtlckey,
				 const struct pubkey *remotehtlckey,
				 const struct brocoin_signature *localhtlcsig,
				 const struct brocoin_signature *remotehtlcsig,
				 const struct preimage *payment_preimage,
				 const struct pubkey *revocationkey,
				 bool option_anchor_outputs)
{
	struct sha256 hash;
	u8 *wscript, **witness;

	sha256(&hash, payment_preimage, sizeof(*payment_preimage));
	wscript = brocoin_wscript_htlc_receive(htlc_success,
					       htlc_abstimeout,
					       localhtlckey, remotehtlckey,
					       &hash, revocationkey,
					       option_anchor_outputs);

	witness = brocoin_witness_htlc_success_tx(htlc_success,
						  localhtlcsig, remotehtlcsig,
						  payment_preimage, wscript);
	brocoin_tx_input_set_witness(htlc_success, 0, take(witness));
	tal_free(wscript);
}

struct brocoin_tx *htlc_timeout_tx(const tal_t *ctx,
				   const struct chainparams *chainparams,
				   const struct brocoin_outpoint *commit,
				   const u8 *commit_wscript,
				   struct amount_mbro htlc_mbronees,
				   u32 cltv_expiry,
				   u16 to_self_delay,
				   u32 feerate_per_kw,
				   const struct keyset *keyset,
				   bool option_anchor_outputs)
{
	/* BOLT #3:
	 * * locktime: `0` for HTLC-success, `cltv_expiry` for HTLC-timeout
	 */
	return htlc_tx(ctx, chainparams, commit,
		       commit_wscript, htlc_mbronees, to_self_delay,
		       &keyset->self_revocation_key,
		       &keyset->self_delayed_payment_key,
		       htlc_timeout_fee(feerate_per_kw,
					option_anchor_outputs),
		       cltv_expiry,
		       option_anchor_outputs);
}

/* Fill in the witness for HTLC-timeout tx produced above. */
void htlc_timeout_tx_add_witness(struct brocoin_tx *htlc_timeout,
				 const struct pubkey *localhtlckey,
				 const struct pubkey *remotehtlckey,
				 const struct sha256 *payment_hash,
				 const struct pubkey *revocationkey,
				 const struct brocoin_signature *localhtlcsig,
				 const struct brocoin_signature *remotehtlcsig,
				 bool option_anchor_outputs)
{
	u8 **witness;
	u8 *wscript = brocoin_wscript_htlc_offer(htlc_timeout,
						 localhtlckey, remotehtlckey,
						 payment_hash, revocationkey,
						 option_anchor_outputs);

	witness = brocoin_witness_htlc_timeout_tx(htlc_timeout, localhtlcsig,
						  remotehtlcsig, wscript);
	brocoin_tx_input_set_witness(htlc_timeout, 0, take(witness));
	tal_free(wscript);
}

u8 *htlc_offered_wscript(const tal_t *ctx,
			 const struct ripemd160 *ripemd,
			 const struct keyset *keyset,
			 bool option_anchor_outputs)
{
	return brocoin_wscript_htlc_offer_ripemd160(ctx,
						    &keyset->self_htlc_key,
						    &keyset->other_htlc_key,
						    ripemd,
						    &keyset->self_revocation_key,
						    option_anchor_outputs);
}

u8 *htlc_received_wscript(const tal_t *ctx,
			  const struct ripemd160 *ripemd,
			  const struct abs_locktime *expiry,
			  const struct keyset *keyset,
			  bool option_anchor_outputs)
{
	return brocoin_wscript_htlc_receive_ripemd(ctx,
						   expiry,
						   &keyset->self_htlc_key,
						   &keyset->other_htlc_key,
						   ripemd,
						   &keyset->self_revocation_key,
						   option_anchor_outputs);
}
