#include "config.h"
#include <assert.h>
#include <common/penalty_base.h>
#include <wire/wire.h>

/* txout must be within tx! */
struct penalty_base *penalty_base_new(const tal_t *ctx,
				      u64 commitment_num,
				      const struct brocoin_tx *tx,
				      const struct wally_tx_output *txout)
{
	struct penalty_base *pbase = tal(ctx, struct penalty_base);

	pbase->commitment_num = commitment_num;
	brocoin_txid(tx, &pbase->txid);
	pbase->outnum = txout - tx->wtx->outputs;
	assert(pbase->outnum < tx->wtx->num_outputs);
	pbase->amount = amount_bro(txout->bronees);

	return pbase;
}

void towire_penalty_base(u8 **pptr, const struct penalty_base *pbase)
{
	towire_u64(pptr, pbase->commitment_num);
	towire_brocoin_txid(pptr, &pbase->txid);
	towire_u32(pptr, pbase->outnum);
	towire_amount_bro(pptr, pbase->amount);
}

void fromwire_penalty_base(const u8 **pptr, size_t *max,
			   struct penalty_base *pbase)
{
	pbase->commitment_num = fromwire_u64(pptr, max);
	fromwire_brocoin_txid(pptr, max, &pbase->txid);
	pbase->outnum = fromwire_u32(pptr, max);
	pbase->amount = fromwire_amount_bro(pptr, max);
}
