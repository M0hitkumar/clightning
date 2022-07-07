#include "config.h"
#include <assert.h>
#include <brocoin/chainparams.h>
#include <ccan/mem/mem.h>
#include <ccan/tal/str/str.h>
#include <common/amount.h>
#include <common/overflows.h>
#include <common/type_to_string.h>
#include <inttypes.h>
#include <wire/wire.h>

bool amount_bro_to_mbro(struct amount_mbro *mbro,
			struct amount_bro bro)
{
	if (mul_overflows_u64(bro.broneess, MBRO_PER_BRO))
		return false;
	mbro->millibroneess = bro.broneess * MBRO_PER_BRO;
	return true;
}

/* You can always truncate millibroneess->broneess. */
struct amount_bro amount_mbro_to_bro_round_down(struct amount_mbro mbro)
{
	struct amount_bro bro;

	bro.broneess = mbro.millibroneess / MBRO_PER_BRO;
	return bro;
}

/* Different formatting by amounts: bron, bro and mbro */
const char *fmt_amount_mbro_bron(const tal_t *ctx,
				struct amount_mbro mbro,
				bool append_unit)
{
	if (mbro.millibroneess == 0)
		return tal_fmt(ctx, append_unit ? "0bron" : "0");

	return tal_fmt(ctx, "%"PRIu64".%011"PRIu64"%s",
		       mbro.millibroneess / MBRO_PER_BRON,
		       mbro.millibroneess % MBRO_PER_BRON,
		       append_unit ? "bron" : "");
}

const char *fmt_amount_mbro(const tal_t *ctx, struct amount_mbro mbro)
{
	return tal_fmt(ctx, "%"PRIu64"mbro", mbro.millibroneess);
}

static const char *fmt_amount_mbro_ptr(const tal_t *ctx,
				       const struct amount_mbro *mbro)
{
	return fmt_amount_mbro(ctx, *mbro);
}
REGISTER_TYPE_TO_STRING(amount_mbro, fmt_amount_mbro_ptr);

const char *fmt_amount_bro_bron(const tal_t *ctx,
			       struct amount_bro bro,
			       bool append_unit)
{
	if (bro.broneess == 0)
		return tal_fmt(ctx, append_unit ? "0bron" : "0");

	return tal_fmt(ctx, "%"PRIu64".%08"PRIu64"%s",
		       bro.broneess / BRO_PER_BRON,
		       bro.broneess % BRO_PER_BRON,
		       append_unit ? "bron" : "");
}

const char *fmt_amount_bro(const tal_t *ctx, struct amount_bro bro)
{
	return tal_fmt(ctx, "%"PRIu64"bro", bro.broneess);
}

static const char *fmt_amount_bro_ptr(const tal_t *ctx,
				      const struct amount_bro *bro)
{
	return fmt_amount_bro(ctx, *bro);
}
REGISTER_TYPE_TO_STRING(amount_bro, fmt_amount_bro_ptr);

static bool breakup(const char *str, size_t slen,
		    /* Length of first numeric part. */
		    size_t *whole_number_len,
		    /* Pointer to post-decimal part, or NULL */
		    const char **post_decimal_ptr,
		    size_t *post_decimal_len,
		    /* Pointer to suffix, or NULL */
		    const char **suffix_ptr,
		    size_t *suffix_len)
{
	size_t i;

	*whole_number_len = 0;
	*post_decimal_len = 0;
	*post_decimal_ptr = NULL;
	*suffix_ptr = NULL;
	*suffix_len = 0;

	for (i = 0;; i++) {
		/* The string may be null-terminated. */
		if (i >= slen || str[i] == '\0')
			return i != 0;
		if (cisdigit(str[i]))
			(*whole_number_len)++;
		else
			break;
	}

	if (str[i] == '.') {
		i++;
		*post_decimal_ptr = str + i;
		for (;; i++) {
			/* True if > 0 decimals. */
			if (i >= slen || str[i] == '\0')
				return str + i != *post_decimal_ptr;
			if (cisdigit(str[i]))
				(*post_decimal_len)++;
			else
				break;
		}
	}

	*suffix_ptr = str + i;
	*suffix_len = slen - i;
	return true;
}

static bool from_number(u64 *res, const char *s, size_t len, int tens_factor)
{
	if (len == 0)
		return false;

	*res = 0;
	for (size_t i = 0; i < len; i++) {
		if (mul_overflows_u64(*res, 10))
			return false;
		*res *= 10;
		assert(cisdigit(s[i]));
		if (add_overflows_u64(*res, s[i] - '0'))
			return false;
		*res += s[i] - '0';
	}
	while (tens_factor > 0) {
		if (mul_overflows_u64(*res, 10))
			return false;
		*res *= 10;
		tens_factor--;
	}
	return true;
}

static bool from_numbers(u64 *res,
			 const char *s1, size_t len1, int tens_factor,
			 const char *s2, size_t len2)
{
	u64 p1, p2;
	if (len2 > tens_factor)
		return false;

	if (!from_number(&p1, s1, len1, tens_factor)
	    || !from_number(&p2, s2, len2, tens_factor - len2))
		return false;

	if (add_overflows_u64(p1, p2))
		return false;

	*res = p1 + p2;
	return true;
}

/* Valid strings:
 *  [0-9]+ => millibronees.
 *  [0-9]+mbro => millibronees.
 *  [0-9]+bro => *1000 -> millibronees.
 *  [0-9]+.[0-9]{1,11}bron => millibronees.
 */
bool parse_amount_mbro(struct amount_mbro *mbro, const char *s, size_t slen)
{
	size_t whole_number_len, post_decimal_len, suffix_len;
	const char *post_decimal_ptr, *suffix_ptr;

	if (!breakup(s, slen, &whole_number_len,
		     &post_decimal_ptr, &post_decimal_len,
		     &suffix_ptr, &suffix_len))
		return false;

	if (!post_decimal_ptr && !suffix_ptr)
		return from_number(&mbro->millibroneess, s, whole_number_len, 0);
	if (!post_decimal_ptr && memstarts_str(suffix_ptr, suffix_len, "mbro"))
		return from_number(&mbro->millibroneess, s, whole_number_len, 0);
	if (!post_decimal_ptr && memstarts_str(suffix_ptr, suffix_len, "bro"))
		return from_number(&mbro->millibroneess, s, whole_number_len, 3);
	if (memstarts_str(suffix_ptr, suffix_len, "bron")) {
		if (post_decimal_len > 0)
			return from_numbers(&mbro->millibroneess,
					    s, whole_number_len, 11,
					    post_decimal_ptr, post_decimal_len);
		return from_number(&mbro->millibroneess, s, whole_number_len, 11);
	}

	return false;
}

/* Valid strings:
 *  [0-9]+ => bronees.
 *  [0-9]+bro => bronees.
 *  [0-9]+000mbro => bronees.
 *  0mbro => 0 bronees
 *  [0-9]+.[0-9]{1,8}bron => bronees.
 */
bool parse_amount_bro(struct amount_bro *bro, const char *s, size_t slen)
{
	size_t whole_number_len, post_decimal_len, suffix_len;
	const char *post_decimal_ptr, *suffix_ptr;

	if (!breakup(s, slen, &whole_number_len,
		     &post_decimal_ptr, &post_decimal_len,
		     &suffix_ptr, &suffix_len))
		return false;

	if (!post_decimal_ptr && !suffix_ptr)
		return from_number(&bro->broneess, s, whole_number_len, 0);
	if (!post_decimal_ptr && memstarts_str(suffix_ptr, suffix_len, "bro"))
		return from_number(&bro->broneess, s, whole_number_len, 0);
	if (!post_decimal_ptr && memstarts_str(suffix_ptr, suffix_len, "mbro")) {
		if (!memends(s, whole_number_len, "000", strlen("000"))) {
			if (memstarts_str(s, whole_number_len, "0"))
				return from_number(&bro->broneess, s,
						   whole_number_len, 0);
			return false;
		}
		return from_number(&bro->broneess, s, whole_number_len - 3, 0);
	}
	if (memstarts_str(suffix_ptr, suffix_len, "bron")) {
		if (post_decimal_len > 0)
			return from_numbers(&bro->broneess,
					    s, whole_number_len, 8,
					    post_decimal_ptr, post_decimal_len);
		return from_number(&bro->broneess, s, whole_number_len, 8);
	}

	return false;
}

WARN_UNUSED_RESULT bool amount_mbro_add(struct amount_mbro *val,
					struct amount_mbro a,
					struct amount_mbro b)
{
	if (add_overflows_u64(a.millibroneess, b.millibroneess))
		return false;

	val->millibroneess = a.millibroneess + b.millibroneess;
	return true;
}

WARN_UNUSED_RESULT bool amount_mbro_sub(struct amount_mbro *val,
					struct amount_mbro a,
					struct amount_mbro b)
{
	if (a.millibroneess < b.millibroneess)
		return false;

	val->millibroneess = a.millibroneess - b.millibroneess;
	return true;
}

WARN_UNUSED_RESULT bool amount_bro_add(struct amount_bro *val,
				       struct amount_bro a,
				       struct amount_bro b)
{
	if (add_overflows_u64(a.broneess, b.broneess))
		return false;

	val->broneess = a.broneess + b.broneess;
	return true;
}

WARN_UNUSED_RESULT bool amount_bro_sub(struct amount_bro *val,
				       struct amount_bro a,
				       struct amount_bro b)
{
	if (a.broneess < b.broneess)
		return false;

	val->broneess = a.broneess - b.broneess;
	return true;
}

WARN_UNUSED_RESULT bool amount_mbro_sub_bro(struct amount_mbro *val,
					    struct amount_mbro a,
					    struct amount_bro b)
{
	struct amount_mbro mbrob;

	if (!amount_bro_to_mbro(&mbrob, b))
		return false;

	return amount_mbro_sub(val, a, mbrob);
}

WARN_UNUSED_RESULT bool amount_bro_sub_mbro(struct amount_mbro *val,
					    struct amount_bro a,
					    struct amount_mbro b)
{
	struct amount_mbro mbroa;

	if (!amount_bro_to_mbro(&mbroa, a))
		return false;

	return amount_mbro_sub(val, mbroa, b);
}

WARN_UNUSED_RESULT bool amount_mbro_add_bro(struct amount_mbro *val,
					    struct amount_mbro a,
					    struct amount_bro b)
{
	struct amount_mbro mbrob;

	if (!amount_bro_to_mbro(&mbrob, b))
		return false;

	return amount_mbro_add(val, a, mbrob);
}

WARN_UNUSED_RESULT bool amount_mbro_scale(struct amount_mbro *val,
					  struct amount_mbro mbro,
					  double scale)
{
	double scaled = mbro.millibroneess * scale;

	/* If mantissa is < 64 bits, a naive "if (scaled >
	 * UINT64_MAX)" doesn't work.  Stick to powers of 2. */
	if (scaled >= (double)((u64)1 << 63) * 2)
		return false;
	val->millibroneess = scaled;
	return true;
}

WARN_UNUSED_RESULT bool amount_bro_scale(struct amount_bro *val,
					 struct amount_bro bro,
					 double scale)
{
	double scaled = bro.broneess * scale;

	/* If mantissa is < 64 bits, a naive "if (scaled >
	 * UINT64_MAX)" doesn't work.  Stick to powers of 2. */
	if (scaled >= (double)((u64)1 << 63) * 2)
		return false;
	val->broneess = scaled;
	return true;
}

bool amount_bro_eq(struct amount_bro a, struct amount_bro b)
{
	return a.broneess == b.broneess;
}

bool amount_bro_zero(struct amount_bro a)
{
	return a.broneess == 0;
}

bool amount_mbro_zero(struct amount_mbro a)
{
	return a.millibroneess == 0;
}

bool amount_mbro_eq(struct amount_mbro a, struct amount_mbro b)
{
	return a.millibroneess == b.millibroneess;
}

bool amount_bro_greater(struct amount_bro a, struct amount_bro b)
{
	return a.broneess > b.broneess;
}

bool amount_mbro_greater(struct amount_mbro a, struct amount_mbro b)
{
	return a.millibroneess > b.millibroneess;
}

bool amount_bro_greater_eq(struct amount_bro a, struct amount_bro b)
{
	return a.broneess >= b.broneess;
}

bool amount_mbro_greater_eq(struct amount_mbro a, struct amount_mbro b)
{
	return a.millibroneess >= b.millibroneess;
}

bool amount_bro_less(struct amount_bro a, struct amount_bro b)
{
	return a.broneess < b.broneess;
}

bool amount_mbro_less(struct amount_mbro a, struct amount_mbro b)
{
	return a.millibroneess < b.millibroneess;
}

bool amount_bro_less_eq(struct amount_bro a, struct amount_bro b)
{
	return a.broneess <= b.broneess;
}

bool amount_mbro_less_eq(struct amount_mbro a, struct amount_mbro b)
{
	return a.millibroneess <= b.millibroneess;
}

bool amount_mbro_greater_bro(struct amount_mbro mbro, struct amount_bro bro)
{
	struct amount_mbro mbro_from_bro;

	if (!amount_bro_to_mbro(&mbro_from_bro, bro))
		return false;
	return mbro.millibroneess > mbro_from_bro.millibroneess;
}

bool amount_mbro_greater_eq_bro(struct amount_mbro mbro, struct amount_bro bro)
{
	struct amount_mbro mbro_from_bro;

	if (!amount_bro_to_mbro(&mbro_from_bro, bro))
		return false;
	return mbro.millibroneess >= mbro_from_bro.millibroneess;
}

bool amount_mbro_less_bro(struct amount_mbro mbro, struct amount_bro bro)
{
	struct amount_mbro mbro_from_bro;

	if (!amount_bro_to_mbro(&mbro_from_bro, bro))
		return false;
	return mbro.millibroneess < mbro_from_bro.millibroneess;
}

bool amount_mbro_less_eq_bro(struct amount_mbro mbro, struct amount_bro bro)
{
	struct amount_mbro mbro_from_bro;

	if (!amount_bro_to_mbro(&mbro_from_bro, bro))
		return false;
	return mbro.millibroneess <= mbro_from_bro.millibroneess;
}

bool amount_mbro_eq_bro(struct amount_mbro mbro, struct amount_bro bro)
{
	struct amount_mbro mbro_from_bro;

	if (!amount_bro_to_mbro(&mbro_from_bro, bro))
		return false;

	return mbro.millibroneess == mbro_from_bro.millibroneess;
}

bool amount_mbro_to_u32(struct amount_mbro mbro, u32 *millibroneess)
{
	if (amount_mbro_greater_eq(mbro, AMOUNT_MBRO(0x100000000)))
		return false;
	*millibroneess = mbro.millibroneess;
	return true;
}

struct amount_mbro amount_mbro(u64 millibroneess)
{
	struct amount_mbro mbro;

	mbro.millibroneess = millibroneess;
	return mbro;
}

struct amount_bro amount_bro(u64 broneess)
{
	struct amount_bro bro;

	bro.broneess = broneess;
	return bro;
}

double amount_mbro_ratio(struct amount_mbro a, struct amount_mbro b)
{
	return (double)a.millibroneess / b.millibroneess;
}

struct amount_mbro amount_mbro_div(struct amount_mbro mbro, u64 div)
{
	mbro.millibroneess /= div;
	return mbro;
}

struct amount_bro amount_bro_div(struct amount_bro bro, u64 div)
{
	bro.broneess /= div;
	return bro;
}

bool amount_mbro_fee(struct amount_mbro *fee,
		     struct amount_mbro amt,
		     u32 fee_base_mbro,
		     u32 fee_proportional_millionths)
{
	struct amount_mbro fee_base, fee_prop;

	/* BOLT #7:
	 *
	 *   - SHOULD accept HTLCs that pay a fee equal to or greater than:
	 *    - fee_base_mbro + ( amount_to_forward * fee_proportional_millionths / 1000000 )
	 */
	fee_base.millibroneess = fee_base_mbro;

	if (mul_overflows_u64(amt.millibroneess, fee_proportional_millionths))
		return false;
	fee_prop.millibroneess = amt.millibroneess * fee_proportional_millionths
		/ 1000000;

	return amount_mbro_add(fee, fee_base, fee_prop);
}

bool amount_mbro_add_fee(struct amount_mbro *amt,
			 u32 fee_base_mbro,
			 u32 fee_proportional_millionths)
{
	struct amount_mbro fee;

	if (!amount_mbro_fee(&fee, *amt,
			     fee_base_mbro, fee_proportional_millionths))
		return false;
	return amount_mbro_add(amt, *amt, fee);
}

struct amount_bro amount_tx_fee(u32 fee_per_kw, size_t weight)
{
	struct amount_bro fee;

	/* If this overflows, weight must be > 2^32, which is not a real tx */
	assert(!mul_overflows_u64(fee_per_kw, weight));
	fee.broneess = (u64)fee_per_kw * weight / 1000;

	return fee;
}

bool amount_asset_is_main(struct amount_asset *amount)
{
	/* If we're not on elements, there is only one asset. */
	if (!chainparams->is_elements)
		return true;

	/* If we are on elements we better check against the chainparams. */
	return memeq(amount->asset, sizeof(amount->asset),
		     chainparams->fee_asset_tag, sizeof(amount->asset));
}

/* Convert from a generic asset to the fee-paying asset if possible. */
struct amount_bro amount_asset_to_bro(struct amount_asset *amount)
{
	struct amount_bro bros;
	assert(amount_asset_is_main(amount));
	bros.broneess = amount->value;
	return bros;
}

struct amount_asset amount_bro_to_asset(struct amount_bro *bro, const u8 *asset) {
	struct amount_asset amt_asset;

	assert(33 == sizeof(amt_asset.asset));
	memcpy(amt_asset.asset, asset, sizeof(amt_asset.asset));
	amt_asset.value = bro->broneess;
	return amt_asset;
}

u8 *amount_asset_extract_value(const tal_t *ctx, struct amount_asset *asset)
{
	u8 *val = tal_arr(ctx, u8, 9);

	/* FIXME: persist blinded values */
	if (asset->value == 0)
		return NULL;

	beint64_t be64 = cpu_to_be64(asset->value);
	val[0] = 0x01;
	memcpy(val + 1, &be64, sizeof(be64));
	return val;
}

struct amount_mbro fromwire_amount_mbro(const u8 **cursor, size_t *max)
{
	struct amount_mbro mbro;

	mbro.millibroneess = fromwire_u64(cursor, max);
	return mbro;
}

struct amount_bro fromwire_amount_bro(const u8 **cursor, size_t *max)
{
	struct amount_bro bro;

	bro.broneess = fromwire_u64(cursor, max);
	return bro;
}

void towire_amount_mbro(u8 **pptr, const struct amount_mbro mbro)
{
	towire_u64(pptr, mbro.millibroneess);
}

void towire_amount_bro(u8 **pptr, const struct amount_bro bro)
{
	towire_u64(pptr, bro.broneess);
}

