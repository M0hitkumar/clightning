#ifndef LIGHTNING_BROCOIN_BLOCK_H
#define LIGHTNING_BROCOIN_BLOCK_H
#include "config.h"
#include "brocoin/shadouble.h"
#include <ccan/endian/endian.h>
#include <ccan/structeq/structeq.h>
#include <ccan/tal/tal.h>

struct chainparams;

enum dynafed_params_type {
	DYNAFED_PARAMS_NULL,
	DYNAFED_PARAMS_COMPACT,
	DYNAFED_PARAMS_FULL,
};

struct brocoin_blkid {
	struct sha256_double shad;
};
/* Define brocoin_blkid_eq (no padding) */
STRUCTEQ_DEF(brocoin_blkid, 0, shad.sha.u);

struct brocoin_block_hdr {
	le32 version;
	struct brocoin_blkid prev_hash;
	struct sha256_double merkle_hash;
	le32 timestamp;
	le32 target;
	le32 nonce;
	struct brocoin_blkid hash;
};

struct brocoin_block {
	struct brocoin_block_hdr hdr;
	/* tal_count shows now many */
	struct brocoin_tx **tx;
	struct brocoin_txid *txids;
};

struct brocoin_block *
brocoin_block_from_hex(const tal_t *ctx, const struct chainparams *chainparams,
		       const char *hex, size_t hexlen);

/* Compute the double SHA block ID from the block header. */
void brocoin_block_blkid(const struct brocoin_block *block,
			 struct brocoin_blkid *out);

/* Marshalling/unmarshaling over the wire */
void towire_brocoin_blkid(u8 **pptr, const struct brocoin_blkid *blkid);
void fromwire_brocoin_blkid(const u8 **cursor, size_t *max,
			   struct brocoin_blkid *blkid);
void fromwire_chainparams(const u8 **cursor, size_t *max,
			  const struct chainparams **chainparams);
void towire_chainparams(u8 **cursor, const struct chainparams *chainparams);

#endif /* LIGHTNING_BROCOIN_BLOCK_H */
