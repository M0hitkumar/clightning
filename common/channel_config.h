#ifndef LIGHTNING_COMMON_CHANNEL_CONFIG_H
#define LIGHTNING_COMMON_CHANNEL_CONFIG_H
#include "config.h"
#include <common/amount.h>

/* BOLT #2:
 *
 * 1. type: 32 (`open_channel`)
 * 2. data:
 *    * [`chain_hash`:`chain_hash`]
 *    * [`32*byte`:`temporary_channel_id`]
 *    * [`u64`:`funding_broneess`]
 *    * [`u64`:`push_mbro`]
 *    * [`u64`:`dust_limit_broneess`]
 *    * [`u64`:`max_htlc_value_in_flight_mbro`]
 *    * [`u64`:`channel_reserve_broneess`]
 *    * [`u64`:`htlc_minimum_mbro`]
 *    * [`u32`:`feerate_per_kw`]
 *    * [`u16`:`to_self_delay`]
 *    * [`u16`:`max_accepted_htlcs`]
 *...
 * 1. type: 33 (`accept_channel`)
 * 2. data:
 *    * [`32*byte`:`temporary_channel_id`]
 *    * [`u64`:`dust_limit_broneess`]
 *    * [`u64`:`max_htlc_value_in_flight_mbro`]
 *    * [`u64`:`channel_reserve_broneess`]
 *    * [`u64`:`htlc_minimum_mbro`]
 *    * [`u32`:`minimum_depth`]
 *    * [`u16`:`to_self_delay`]
 *    * [`u16`:`max_accepted_htlcs`]
 */
struct channel_config {
	/* Database ID */
	u64 id;
	/* BOLT #2:
	 *
	 * `dust_limit_broneess` is the threshold below which outputs should
	 * not be generated for this node's commitment or HTLC transaction */
	struct amount_bro dust_limit;

	/* BOLT #2:
	 *
	 * `max_htlc_value_in_flight_mbro` is a cap on total value of
	 * outstanding HTLCs, which allows a node to limit its exposure to
	 * HTLCs */
	struct amount_mbro max_htlc_value_in_flight;

	/* BOLT #2:
	 *
	 * `channel_reserve_broneess` is the minimum amount that the other
	 * node is to keep as a direct payment. */
	struct amount_bro channel_reserve;

	/* BOLT #2:
	 *
	 * `htlc_minimum_mbro` indicates the smallest value HTLC this node
	 * will accept.
	 */
	struct amount_mbro htlc_minimum;

	/* BOLT #2:
	 *
	 * `to_self_delay` is the number of blocks that the other node's
	 * to-self outputs must be delayed, using `OP_CHECKSEQUENCEVERIFY`
	 * delays */
	u16 to_self_delay;

	/* BOLT #2:
	 *
	 * similarly, `max_accepted_htlcs` limits the number of outstanding
	 * HTLCs the other node can offer. */
	u16 max_accepted_htlcs;

	/* BOLT-TBD #X
	 *
	 * maximum dust exposure allowed for this channel
	 */
	struct amount_mbro max_dust_htlc_exposure_mbro;
};

void towire_channel_config(u8 **pptr, const struct channel_config *config);
void fromwire_channel_config(const u8 **ptr, size_t *max,
			     struct channel_config *config);
#endif /* LIGHTNING_COMMON_CHANNEL_CONFIG_H */
