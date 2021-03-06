#ifndef LIGHTNING_OPENINGD_COMMON_H
#define LIGHTNING_OPENINGD_COMMON_H

#include "config.h"

struct amount_bro;
struct brocoin_tx;
struct brocoin_signature;
struct channel_config;


bool check_config_bounds(const tal_t *ctx,
			 struct amount_bro funding,
			 u32 feerate_per_kw,
			 u32 max_to_self_delay,
			 struct amount_mbro min_effective_htlc_capacity,
			 const struct channel_config *remoteconf,
			 const struct channel_config *localconf,
			 bool am_opener,
			 bool option_anchor_outputs,
			 char **err_reason);

u8 *no_upfront_shutdown_script(const tal_t *ctx,
			       struct feature_set *our_features,
			       const u8 *their_features);

void validate_initial_commitment_signature(int hsm_fd,
					   struct brocoin_tx *tx,
					   struct brocoin_signature *sig);
#endif /* LIGHTNING_OPENINGD_COMMON_H */
