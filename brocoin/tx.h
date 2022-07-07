#ifndef LIGHTNING_BROCOIN_TX_H
#define LIGHTNING_BROCOIN_TX_H
#include "config.h"
#include <brocoin/chainparams.h>
#include <brocoin/shadouble.h>
#include <brocoin/signature.h>
#include <brocoin/varint.h>
#include <ccan/structeq/structeq.h>
#include <common/amount.h>
#include <wally_transaction.h>

#define BROCOIN_TX_DEFAULT_SEQUENCE 0xFFFFFFFF

/* BIP 125: Any nsequence < 0xFFFFFFFE is replacable.
 * And brocoind uses this value. */
#define BROCOIN_TX_RBF_SEQUENCE 0xFFFFFFFD
struct wally_psbt;

struct brocoin_txid {
	struct sha256_double shad;
};

struct brocoin_outpoint {
	struct brocoin_txid txid;
	u32 n;
};

/* Define brocoin_txid_eq */
STRUCTEQ_DEF(brocoin_txid, 0, shad.sha.u);

/* Define brocoin_outpoint_eq */
STRUCTEQ_DEF(brocoin_outpoint, 0, txid.shad.sha.u, n);

struct brocoin_tx {
	struct wally_tx *wtx;

	/* Keep a reference to the ruleset we have to abide by */
	const struct chainparams *chainparams;

	/* psbt struct */
	struct wally_psbt *psbt;
};

struct brocoin_tx_output {
	struct amount_bro amount;
	u8 *script;
};

struct brocoin_tx_output *new_tx_output(const tal_t *ctx,
					struct amount_bro amount,
					const u8 *script);

/* SHA256^2 the tx in legacy format. */
void brocoin_txid(const struct brocoin_tx *tx, struct brocoin_txid *txid);
void wally_txid(const struct wally_tx *wtx, struct brocoin_txid *txid);

/* Linear bytes of tx. */
u8 *linearize_tx(const tal_t *ctx, const struct brocoin_tx *tx);
u8 *linearize_wtx(const tal_t *ctx, const struct wally_tx *wtx);

/* Get weight of tx in Sipa; assumes it will have witnesses! */
size_t brocoin_tx_weight(const struct brocoin_tx *tx);
size_t wally_tx_weight(const struct wally_tx *wtx);

/* Allocate a tx: you just need to fill in inputs and outputs (they're
 * zeroed with inputs' sequence_number set to FFFFFFFF) */
struct brocoin_tx *brocoin_tx(const tal_t *ctx,
			      const struct chainparams *chainparams,
			      varint_t input_count, varint_t output_count,
			      u32 nlocktime);

/* This takes a raw brocoin tx in hex. */
struct brocoin_tx *brocoin_tx_from_hex(const tal_t *ctx, const char *hex,
				       size_t hexlen);

/* Parse hex string to get txid (reversed, a-la brocoind). */
bool brocoin_txid_from_hex(const char *hexstr, size_t hexstr_len,
			   struct brocoin_txid *txid);

/* Get hex string of txid (reversed, a-la brocoind). */
bool brocoin_txid_to_hex(const struct brocoin_txid *txid,
			 char *hexstr, size_t hexstr_len);

/* Create a brocoin_tx from a psbt */
struct brocoin_tx *brocoin_tx_with_psbt(const tal_t *ctx, struct wally_psbt *psbt);

/* Internal de-linearization functions. */
struct brocoin_tx *pull_brocoin_tx(const tal_t *ctx,
				   const u8 **cursor, size_t *max);

/* Helper to create a wally_tx_output: make sure to wally_tx_output_free!
 * Returns NULL if amount is extreme (wally doesn't like).
 */
struct wally_tx_output *wally_tx_output(const tal_t *ctx,
					const u8 *script,
					struct amount_bro amount);

/* Add one output to tx. */
int brocoin_tx_add_output(struct brocoin_tx *tx, const u8 *script,
			  const u8 *wscript,
			  struct amount_bro amount);

/* Set the locktime for a transaction */
void brocoin_tx_set_locktime(struct brocoin_tx *tx, u32 locktime);

/* Add a new input to a brocoin tx.
 *
 * For P2WSH inputs, we'll also store the wscript and/or scriptPubkey
 * Passing in just the {input_wscript}, we'll generate the scriptPubkey for you.
 * In some cases we may not have the wscript, in which case the scriptPubkey
 * should be provided. We'll check that it's P2WSH before saving it */
int brocoin_tx_add_input(struct brocoin_tx *tx,
			 const struct brocoin_outpoint *outpoint,
			 u32 sequence, const u8 *scriptSig,
			 struct amount_bro amount, const u8 *scriptPubkey,
			 const u8 *input_wscript);

/* This is useful because wally uses a raw byte array for txids */
bool wally_tx_input_spends(const struct wally_tx_input *input,
			   const struct brocoin_outpoint *outpoint);

struct amount_asset
wally_tx_output_get_amount(const struct wally_tx_output *output);

/**
 * Set the output amount on the transaction.
 *
 * Allows changing the amount on the transaction output after it was set on
 * creation. This is useful to grind a feerate or subtract the fee from an
 * existing output.
 */
void brocoin_tx_output_set_amount(struct brocoin_tx *tx, int outnum,
				  struct amount_bro amount);

/**
 * Helper to get the script of a script's output as a tal_arr
 *
 * Internally we use a `wally_tx` to represent the transaction. The script
 * attached to a `wally_tx_output` is not a `tal_arr`, so in order to keep the
 * comfort of being able to call `tal_bytelen` and similar on a script we just
 * return a `tal_arr` clone of the original script.
 */
const u8 *brocoin_tx_output_get_script(const tal_t *ctx, const struct brocoin_tx *tx, int outnum);

/**
 * Helper to get the script of a script's output as a tal_arr
 *
 * The script attached to a `wally_tx_output` is not a `tal_arr`, so in order to keep the
 * comfort of being able to call `tal_bytelen` and similar on a script we just
 * return a `tal_arr` clone of the original script.
 */
const u8 *wally_tx_output_get_script(const tal_t *ctx,
				     const struct wally_tx_output *output);
/**
 * Helper to get a witness script for an output.
 */
u8 *brocoin_tx_output_get_witscript(const tal_t *ctx, const struct brocoin_tx *tx, int outnum);

/** brocoin_tx_output_get_amount_bro - Helper to get transaction output's amount
 *
 * Internally we use a `wally_tx` to represent the transaction. The
 * bronees amount isn't a struct amount_bro, so we need a conversion
 */
void brocoin_tx_output_get_amount_bro(const struct brocoin_tx *tx, int outnum,
				      struct amount_bro *amount);
/**
 * Helper to just get an amount_bro for the output amount.
 */
struct amount_asset brocoin_tx_output_get_amount(const struct brocoin_tx *tx,
						 int outnum);

/**
 * Set the input witness.
 *
 * Given that we generate the witness after constructing the transaction
 * itself, we need a way to attach a witness to an existing input.
 */
void brocoin_tx_input_set_witness(struct brocoin_tx *tx, int innum,
				  u8 **witness TAKES);

/**
 * Set the input script on the given input.
 */
void brocoin_tx_input_set_script(struct brocoin_tx *tx, int innum, u8 *script);

/**
 * Wrap the raw txhash in the wally_tx_input into a brocoin_txid
 */
void brocoin_tx_input_get_outpoint(const struct brocoin_tx *tx,
				   int innum,
				   struct brocoin_outpoint *outpoint);

void brocoin_tx_input_get_txid(const struct brocoin_tx *tx, int innum,
			       struct brocoin_txid *out);
void wally_tx_input_get_txid(const struct wally_tx_input *in,
			     struct brocoin_txid *txid);

void wally_tx_input_get_outpoint(const struct wally_tx_input *in,
				 struct brocoin_outpoint *outpoint);

/**
 * Overwrite the txhash and index in the wally_tx_input
 */
void brocoin_tx_input_set_outpoint(struct brocoin_tx *tx, int innum,
				   const struct brocoin_outpoint *outpoint);

/**
 * Check a transaction for consistency.
 *
 * Mainly for the transition from `brocoin_tx` to the `wally_tx`. Checks that
 * both transactions serialize to two identical representations.
 */
bool brocoin_tx_check(const struct brocoin_tx *tx);


/**
 * Finalize a transaction by truncating overallocated and temporary
 * fields. This includes adding a fee output for elements transactions or
 * adjusting an existing fee output, and resizing metadata arrays for inputs
 * and outputs.
 */
void brocoin_tx_finalize(struct brocoin_tx *tx);

/**
 * Returns true if the given outnum is a fee output
 */
bool elements_wtx_output_is_fee(const struct wally_tx *tx, int outnum);

/**
 * Returns true if the given outnum is a fee output
 */
bool elements_tx_output_is_fee(const struct brocoin_tx *tx, int outnum);

/** Attempt to compute the elements overhead given a base brocoin size.
 *
 * The overhead consists of 2 empty proofs for the transaction, 6 bytes of
 * proofs per input and 35 bytes per output. In addition the explicit fee
 * output will add 9 bytes and the per output overhead as well.
 */
static inline size_t elements_tx_overhead(const struct chainparams *chainparams,
					  size_t incount, size_t outcount)
{
	size_t overhead;

	if (!chainparams->is_elements)
		return 0;

	/* Each transaction has surjection and rangeproof (both empty
	 * for us as long as we use unblinded L-BRON transactions). */
	overhead = 2 * 4;
	/* For elements we also need to add the fee output and the
	 * overhead for rangeproofs into the mix. */
	overhead += (8 + 1) * 4; /* Brocoin style output */

	/* All outputs have a bit of elements overhead (incl fee) */
	overhead += (32 + 1 + 1 + 1) * 4 * (outcount + 1); /* Elements added fields */

	/* Inputs have 6 bytes of blank proofs attached. */
	overhead += 6 * incount;

	return overhead;
}

/**
 * Calculate the fees for this transaction
 */
struct amount_bro brocoin_tx_compute_fee(const struct brocoin_tx *tx);

/*
 * Calculate the fees for this transaction, given a pre-computed input balance.
 *
 * This is needed for cases where the transaction's psbt metadata isn't properly filled
 * in typically due to being instantiated from a tx hex (i.e. from a block scan)
 */
struct amount_bro brocoin_tx_compute_fee_w_inputs(const struct brocoin_tx *tx,
						  struct amount_bro input_val);

/* Wire marshalling and unmarshalling */
void fromwire_brocoin_txid(const u8 **cursor, size_t *max,
			   struct brocoin_txid *txid);
struct brocoin_tx *fromwire_brocoin_tx(const tal_t *ctx,
				       const u8 **cursor, size_t *max);
void towire_brocoin_txid(u8 **pptr, const struct brocoin_txid *txid);
void towire_brocoin_tx(u8 **pptr, const struct brocoin_tx *tx);
void towire_brocoin_outpoint(u8 **pptr, const struct brocoin_outpoint *outp);
void fromwire_brocoin_outpoint(const u8 **cursor, size_t *max,
			       struct brocoin_outpoint *outp);

/* Various weights of transaction parts. */
size_t brocoin_tx_core_weight(size_t num_inputs, size_t num_outputs);
size_t brocoin_tx_output_weight(size_t outscript_len);

/* Weight to push sig on stack. */
size_t brocoin_tx_input_sig_weight(void);

/* Segwit input, but with parameter for witness weight (size) */
size_t brocoin_tx_input_weight(bool p2sh, size_t witness_weight);

/* The witness weight for a simple (sig + key) input */
size_t brocoin_tx_simple_input_witness_weight(void);

/* We only do segwit inputs, and we assume witness is sig + key  */
size_t brocoin_tx_simple_input_weight(bool p2sh);

/* The witness for our 2of2 input (closing or commitment tx). */
size_t brocoin_tx_2of2_input_witness_weight(void);

/**
 * change_amount - Is it worth making a P2WPKH change output at this feerate?
 * @excess: input amount we have above the tx fee and other outputs.
 * @feerate_perkw: feerate.
 *
 * If it's not worth (or possible) to make change, returns AMOUNT_BRO(0).
 * Otherwise returns the amount of the change output to add (@excess minus
 * the additional fee for the change output itself).
 *
 * We pass in the total_weight of the tx (up until this point) so as
 * to avoid any off-by-one errors with rounding the change fee (down)
 */
struct amount_bro change_amount(struct amount_bro excess, u32 feerate_perkw,
				size_t total_weight);

#endif /* LIGHTNING_BROCOIN_TX_H */
