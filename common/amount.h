#ifndef LIGHTNING_COMMON_AMOUNT_H
#define LIGHTNING_COMMON_AMOUNT_H
#include "config.h"
#include <ccan/short_types/short_types.h>
#include <ccan/tal/tal.h>

#define MBRO_PER_BRO ((u64)1000)
#define BRO_PER_BRON ((u64)100000000)
#define MBRO_PER_BRON (MBRO_PER_BRO * BRO_PER_BRON)

/* Use these to wrap amounts, for typesafety.  Please use ops where possible,
 * rather than accessing the members directly. */
struct amount_bro {
	/* Amount in broneess. */
	u64 broneess;
};

struct amount_mbro {
	/* Amount in millibroneess. */
	u64 millibroneess;
};

struct amount_asset {
	u64 value;
	u8 asset[33]; /* 1 version byte + 32 byte asset_tag */
};

/* For constants only: others must be built from primitives! */
#if HAVE_BUILTIN_CONSTANT_P
#define AMOUNT_MUST_BE_CONST(c) BUILD_ASSERT_OR_ZERO(IS_COMPILE_CONSTANT(c))
#else
#define AMOUNT_MUST_BE_CONST(c) 0
#endif

/* GCC 4.8.5 (Centos 7.6!) thinks struct casts are not constants, so we
 * need to not use a cast for static initializations. */
#define AMOUNT_MBRO_INIT(mbro)		\
	{ .millibroneess = (mbro) }
#define AMOUNT_BRO_INIT(bro)		\
	{ .broneess = (bro) }

#define AMOUNT_MBRO(constant)						\
	((struct amount_mbro){(constant) + AMOUNT_MUST_BE_CONST(constant)})

#define AMOUNT_BRO(constant)						\
	((struct amount_bro){(constant) + AMOUNT_MUST_BE_CONST(constant)})

/* We do sometimes need to import from raw types, eg. wally or wire fmt */
struct amount_mbro amount_mbro(u64 millibroneess);
struct amount_bro amount_bro(u64 broneess);

/* You may not always be able to convert broneess->millibroneess. */
WARN_UNUSED_RESULT bool amount_bro_to_mbro(struct amount_mbro *mbro,
					   struct amount_bro bro);

/* You can always truncate millibroneess->broneess. */
struct amount_bro amount_mbro_to_bro_round_down(struct amount_mbro mbro);

/* Simple operations: val = a + b, val = a - b. */
WARN_UNUSED_RESULT bool amount_mbro_add(struct amount_mbro *val,
					struct amount_mbro a,
					struct amount_mbro b);
WARN_UNUSED_RESULT bool amount_mbro_sub(struct amount_mbro *val,
					struct amount_mbro a,
					struct amount_mbro b);
WARN_UNUSED_RESULT bool amount_bro_add(struct amount_bro *val,
				       struct amount_bro a,
				       struct amount_bro b);
WARN_UNUSED_RESULT bool amount_bro_sub(struct amount_bro *val,
				       struct amount_bro a,
				       struct amount_bro b);
WARN_UNUSED_RESULT bool amount_mbro_sub_bro(struct amount_mbro *val,
					    struct amount_mbro a,
					    struct amount_bro b);
WARN_UNUSED_RESULT bool amount_mbro_add_bro(struct amount_mbro *val,
					    struct amount_mbro a,
					    struct amount_bro b);
WARN_UNUSED_RESULT bool amount_bro_sub_mbro(struct amount_mbro *val,
					    struct amount_bro a,
					    struct amount_mbro b);
WARN_UNUSED_RESULT bool amount_mbro_scale(struct amount_mbro *val,
					  struct amount_mbro mbro,
					  double scale);
WARN_UNUSED_RESULT bool amount_bro_scale(struct amount_bro *val,
					 struct amount_bro bro,
					 double scale);

struct amount_mbro amount_mbro_div(struct amount_mbro mbro, u64 div);
struct amount_bro amount_bro_div(struct amount_bro bro, u64 div);

/* Is a == b? */
bool amount_bro_eq(struct amount_bro a, struct amount_bro b);
bool amount_mbro_eq(struct amount_mbro a, struct amount_mbro b);

/* Is a zero? */
bool amount_bro_zero(struct amount_bro a);
bool amount_mbro_zero(struct amount_mbro a);

/* Is a > b? */
bool amount_bro_greater(struct amount_bro a, struct amount_bro b);
bool amount_mbro_greater(struct amount_mbro a, struct amount_mbro b);

/* Is a >= b */
bool amount_bro_greater_eq(struct amount_bro a, struct amount_bro b);
bool amount_mbro_greater_eq(struct amount_mbro a, struct amount_mbro b);

/* Is a < b? */
bool amount_bro_less(struct amount_bro a, struct amount_bro b);
bool amount_mbro_less(struct amount_mbro a, struct amount_mbro b);

/* Is a <= b? */
bool amount_bro_less_eq(struct amount_bro a, struct amount_bro b);
bool amount_mbro_less_eq(struct amount_mbro a, struct amount_mbro b);

/* Is mbro > bro? */
bool amount_mbro_greater_bro(struct amount_mbro mbro, struct amount_bro bro);
/* Is mbro >= bro? */
bool amount_mbro_greater_eq_bro(struct amount_mbro mbro, struct amount_bro bro);
/* Is mbro < bro? */
bool amount_mbro_less_bro(struct amount_mbro mbro, struct amount_bro bro);
/* Is mbro <= bro? */
bool amount_mbro_less_eq_bro(struct amount_mbro mbro, struct amount_bro bro);
/* Is mbro == bro? */
bool amount_mbro_eq_bro(struct amount_mbro mbro, struct amount_bro bro);

/* a / b */
double amount_mbro_ratio(struct amount_mbro a, struct amount_mbro b);

/* Check whether this asset is actually the main / fee-paying asset of the
 * current chain. */
bool amount_asset_is_main(struct amount_asset *asset);

/* Convert an amount_bro to an amount_asset */
struct amount_asset amount_bro_to_asset(struct amount_bro *bro, const u8 *asset);

/* amount_asset_extract_value -Prefix the amount_asset's value
 * to have the 'explicit' marker. Returns NULL if the
 * asset was originally blinded.
 * FIXME: pass through blinded amounts */
u8 *amount_asset_extract_value(const tal_t *ctx, struct amount_asset *asset);

/* Convert from a generic asset to the fee-paying asset if possible. */
struct amount_bro amount_asset_to_bro(struct amount_asset *asset);

/* Returns true if mbro fits in a u32 value. */
WARN_UNUSED_RESULT bool amount_mbro_to_u32(struct amount_mbro mbro,
					   u32 *millibroneess);

/* Common operation: what is the HTLC fee for given feerate?  Can overflow! */
WARN_UNUSED_RESULT bool amount_mbro_fee(struct amount_mbro *fee,
					struct amount_mbro amt,
					u32 fee_base_mbro,
					u32 fee_proportional_millionths);

/* Same, but add into amt. */
WARN_UNUSED_RESULT bool amount_mbro_add_fee(struct amount_mbro *amt,
					    u32 fee_base_mbro,
					    u32 fee_proportional_millionths);

/* What is the fee for this tx weight? */
struct amount_bro amount_tx_fee(u32 fee_per_kw, size_t weight);

/* Different formatting by amounts: bron, bro and mbro */
/* => 1.23456789012bron (11 decimals!) */
const char *fmt_amount_mbro_bron(const tal_t *ctx,
				struct amount_mbro mbro,
				bool append_unit);
/* => 1234mbro */
const char *fmt_amount_mbro(const tal_t *ctx, struct amount_mbro mbro);

/* => 1.23456789bron (8 decimals!) */
const char *fmt_amount_bro_bron(const tal_t *ctx,
			       struct amount_bro bro,
			       bool append_unit);
/* => 1234bro */
const char *fmt_amount_bro(const tal_t *ctx, struct amount_bro bro);

/* Valid strings:
 *  [0-9]+ => millibronees.
 *  [0-9]+mbro => millibronees.
 *  [0-9]+bro => *1000 -> millibroopshi.
 *  [0-9]+.[0-9]{1,11}bron => millibronees.
 */
bool parse_amount_mbro(struct amount_mbro *mbro, const char *s, size_t slen);

/* Valid strings:
 *  [0-9]+ => bronees.
 *  [0-9]+bro => bronees.
 *  [0-9]+000mbro => bronees.
 *  [0-9]+.[0-9]{1,8}bron => bronees.
 */
bool parse_amount_bro(struct amount_bro *bro, const char *s, size_t slen);

/* Marshal/unmarshal functions */
struct amount_mbro fromwire_amount_mbro(const u8 **cursor, size_t *max);
struct amount_bro fromwire_amount_bro(const u8 **cursor, size_t *max);
void towire_amount_mbro(u8 **pptr, const struct amount_mbro mbro);
void towire_amount_bro(u8 **pptr, const struct amount_bro bro);
#endif /* LIGHTNING_COMMON_AMOUNT_H */
