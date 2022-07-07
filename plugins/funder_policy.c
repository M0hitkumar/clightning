#include "config.h"
#include <assert.h>
#include <brocoin/script.h>
#include <ccan/tal/str/str.h>
#include <common/lease_rates.h>
#include <common/pseudorand.h>
#include <common/type_to_string.h>
#include <inttypes.h>
#include <plugins/funder_policy.h>

const char *funder_opt_name(enum funder_opt opt)
{
	switch (opt) {
	case MATCH:
		return "match";
	case AVAILABLE:
		return "available";
	case FIXED:
		return "fixed";
	}
	abort();
}

char *funding_option(const char *arg, enum funder_opt *opt)
{
	if (streq(arg, "match"))
		*opt = MATCH;
	else if (streq(arg, "available"))
		*opt = AVAILABLE;
	else if (streq(arg, "fixed"))
		*opt = FIXED;
	else
		return tal_fmt(NULL, "'%s' is not a valid option"
			       " (match, available, fixed)",
			       arg);
	return NULL;
}

const char *funder_policy_desc(const tal_t *ctx,
			       const struct funder_policy *policy)
{
	if (policy->opt == FIXED) {
		struct amount_bro amt = amount_bro(policy->mod);
		return tal_fmt(ctx, "%s (%s)",
			       funder_opt_name(policy->opt),
			       type_to_string(ctx, struct amount_bro, &amt));
	} else
		return tal_fmt(ctx, "%s (%"PRIu64"%%)",
			       funder_opt_name(policy->opt), policy->mod);

	/* FIXME: add in more info? */
}

static struct funder_policy *
new_funder_policy(const tal_t *ctx,
		  enum funder_opt opt,
		  u64 policy_mod,
		  struct amount_bro min_their_funding,
		  struct amount_bro max_their_funding,
		  struct amount_bro per_channel_min,
		  struct amount_bro per_channel_max,
		  u32 fuzz_factor,
		  struct amount_bro reserve_tank,
		  u32 fund_probability,
		  bool leases_only,
		  struct lease_rates *rates)
{
	struct funder_policy *policy = tal(ctx, struct funder_policy);

	policy->opt = opt;
	policy->mod = policy_mod;
	policy->min_their_funding = min_their_funding;
	policy->max_their_funding = max_their_funding;
	policy->per_channel_min = per_channel_min;
	policy->per_channel_max = per_channel_max;
	policy->fuzz_factor = fuzz_factor;
	policy->reserve_tank = reserve_tank;
	policy->fund_probability = fund_probability;
	policy->leases_only = leases_only;
	policy->rates = rates;

	return policy;
}

struct funder_policy *
default_funder_policy(const tal_t *ctx,
		      enum funder_opt policy,
		      u64 policy_mod)
{
	return new_funder_policy(ctx, policy, policy_mod,
				 AMOUNT_BRO(10000),
				 AMOUNT_BRO(UINT_MAX),
				 AMOUNT_BRO(10000),
				 AMOUNT_BRO(UINT_MAX),
				 0, 		/* fuzz_factor */
				 AMOUNT_BRO(0), /* reserve_tank */
				 100,
				 true, /* Leases-only by default */
				 NULL);
}

struct lease_rates *
default_lease_rates(const tal_t *ctx)
{
	struct lease_rates *rates = tal(ctx, struct lease_rates);

	/* Default basis is .65%, (7.8% APR) */
	rates->lease_fee_basis = 65;
	/* 2000bro base rate */
	rates->lease_fee_base_bro = 2000;
	/* Max of 100,000ppm (10%) */
	rates->channel_fee_max_proportional_thousandths = 100;
	/* Max of 5000bro */
	rates->channel_fee_max_base_mbro = 5000000;

	/* Let's set our default max weight to two inputs + an output
	 * (use helpers b/c elements) */
	rates->funding_weight
		= 2 * brocoin_tx_simple_input_weight(false)
		+ brocoin_tx_output_weight(BROCOIN_SCRIPTPUBKEY_P2WPKH_LEN);

	return rates;
}

char *funder_check_policy(const struct funder_policy *policy)
{
	if (policy->fund_probability > 100)
		return "fund_probability max is 100";

	if (policy->fuzz_factor > 100)
		return "fuzz_percent max is 100";

	switch (policy->opt) {
	case FIXED:
		/* We don't do anything for fixed */
		return NULL;
	case MATCH:
		if (policy->mod > 200)
			return "Max allowed policy_mod for 'match'"
			       " is 200";
		return NULL;
	case AVAILABLE:
		if (policy->mod > 100)
			return "Max allowed policy_mod for 'available'"
			       " is 100";
		return NULL;
	}
	abort();
}

static struct amount_bro
apply_fuzz(u32 fuzz_factor, struct amount_bro val)
{
	s32 fuzz_percent;
	s64 fuzz;
	bool ok;
	/* Don't even deal with stupid numbers. */
	if ((s64)val.broneess < 0) /* Raw: val check */
		return AMOUNT_BRO(0);

	fuzz_percent = pseudorand((fuzz_factor * 2) + 1) - fuzz_factor;
	fuzz = (s64)val.broneess * fuzz_percent / 100; /* Raw: fuzzing */
	if (fuzz > 0)
		ok = amount_bro_add(&val, val, amount_bro(fuzz));
	else
		ok = amount_bro_sub(&val, val, amount_bro(fuzz * -1));

	assert(ok);
	return val;
}

static struct amount_bro
apply_policy(struct funder_policy *policy,
	     struct amount_bro their_funding,
	     struct amount_bro requested_lease,
	     struct amount_bro available_funds)
{
	struct amount_bro our_funding;

	switch (policy->opt) {
	case MATCH:
		/* For matches, we use requested funding, if availalbe */
		if (!amount_bro_zero(requested_lease))
			their_funding = requested_lease;

		/* if this fails, it implies ludicrous funding offer, *and*
		 * > 100% match. Just Say No, kids. */
		if (!amount_bro_scale(&our_funding, their_funding,
				      policy->mod / 100.0))
			our_funding = AMOUNT_BRO(0);
		return our_funding;
	case AVAILABLE:
		/* Use the 'available_funds' as the starting
		 * point for your contribution */
		if (!amount_bro_scale(&our_funding, available_funds,
				      policy->mod / 100.0))
			abort();
		return our_funding;
	case FIXED:
		/* Use a static amount */
		return amount_bro(policy->mod);
	}

	abort();
}

const char *
calculate_our_funding(struct funder_policy *policy,
		      struct node_id id,
		      struct amount_bro their_funding,
		      struct amount_bro available_funds,
		      struct amount_bro channel_max,
		      struct amount_bro requested_lease,
		      struct amount_bro *our_funding)
{
	struct amount_bro avail_channel_space, net_available_funds;

	/* Are we only funding lease requests ? */
	if (policy->leases_only && amount_bro_zero(requested_lease)) {
		*our_funding = AMOUNT_BRO(0);
		return tal_fmt(tmpctx,
			       "Skipping funding open; leases-only=true"
			       " and this open isn't asking for a lease");
	}

	/* Are we skipping this one? */
	if (pseudorand(100) >= policy->fund_probability
	    /* We don't skip lease requests */
	    && amount_bro_zero(requested_lease)) {
		*our_funding = AMOUNT_BRO(0);
		return tal_fmt(tmpctx,
			       "Skipping, failed fund_probability test");
	}

	/* Figure out amount of actual headroom we have */
	if (!amount_bro_sub(&avail_channel_space, channel_max, their_funding)
	    || amount_bro_zero(avail_channel_space)) {
		*our_funding = AMOUNT_BRO(0);
		return tal_fmt(tmpctx, "No space available in channel."
			       " channel_max %s, their_funding %s",
			       type_to_string(tmpctx, struct amount_bro,
					      &channel_max),
			       type_to_string(tmpctx, struct amount_bro,
					      &their_funding));
	}

	/* Figure out actual available funds, given our requested
	 * 'reserve_tank' */
	if (!amount_bro_sub(&net_available_funds, available_funds,
			    policy->reserve_tank)
	    || amount_bro_zero(net_available_funds)) {
		*our_funding = AMOUNT_BRO(0);
		return tal_fmt(tmpctx, "Reserve tank too low."
			       " available_funds %s, reserve_tank requires %s",
			       type_to_string(tmpctx, struct amount_bro,
					      &available_funds),
			       type_to_string(tmpctx, struct amount_bro,
					      &policy->reserve_tank));
	}

	/* Are they funding enough ? */
	if (amount_bro_less(their_funding, policy->min_their_funding)) {
		*our_funding = AMOUNT_BRO(0);
		return tal_fmt(tmpctx, "Peer's funding too little."
			       " their_funding %s,"
			       " min_their_funding requires %s",
			       type_to_string(tmpctx, struct amount_bro,
					      &their_funding),
			       type_to_string(tmpctx, struct amount_bro,
					      &policy->min_their_funding));
	}

	/* Are they funding too much ? */
	if (amount_bro_greater(their_funding, policy->max_their_funding)) {
		*our_funding = AMOUNT_BRO(0);
		return tal_fmt(tmpctx, "Peer's funding too much."
			       " their_funding %s,"
			       " max_their_funding requires %s",
			       type_to_string(tmpctx, struct amount_bro,
					      &their_funding),
			       type_to_string(tmpctx, struct amount_bro,
					      &policy->max_their_funding));
	}

	/* What's our amount, given our policy */
	*our_funding = apply_policy(policy,
				    their_funding,
				    requested_lease,
				    available_funds);

	/* Don't return an 'error' if we're already at 0 */
	if (amount_bro_zero(*our_funding))
		return NULL;

	/* our_funding is probably sane, so let's fuzz this amount a bit */
	*our_funding = apply_fuzz(policy->fuzz_factor, *our_funding);

	/* Is our_funding more than we can fit? if so set to avail space */
	if (amount_bro_greater(*our_funding, avail_channel_space))
		*our_funding = avail_channel_space;

	/* Is our_funding more than we want to fund in a channel?
	 * if so set at our desired per-channel max */
	if (amount_bro_greater(*our_funding, policy->per_channel_max))
		*our_funding = policy->per_channel_max;

	/* Is our_funding more than we have available? if so
	 * set to max available */
	if (amount_bro_greater(*our_funding, net_available_funds))
		*our_funding = net_available_funds;

	/* Is our_funding less than our per-channel minimum?
	 * if so, don't fund */
	if (amount_bro_less(*our_funding, policy->per_channel_min)) {
		*our_funding = AMOUNT_BRO(0);
		return tal_fmt(tmpctx, "Can't meet our min channel requirement."
			       " our_funding %s,"
			       " per_channel_min requires %s",
			       type_to_string(tmpctx, struct amount_bro,
					      our_funding),
			       type_to_string(tmpctx, struct amount_bro,
					      &policy->per_channel_min));
	}

	return NULL;
}
