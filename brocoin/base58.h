#ifndef LIGHTNING_BROCOIN_BASE58_H
#define LIGHTNING_BROCOIN_BASE58_H
#include "config.h"

#include <brocoin/chainparams.h>
#include <ccan/crypto/ripemd160/ripemd160.h>

struct pubkey;
struct privkey;
struct brocoin_address;

/* Brocoin address encoded in base58, with version and checksum */
char *brocoin_to_base58(const tal_t *ctx, const struct chainparams *chainparams,
			const struct brocoin_address *addr);

/* P2SH address encoded as base58, with version and checksum */
char *p2sh_to_base58(const tal_t *ctx, const struct chainparams *chainparams,
		     const struct ripemd160 *p2sh);

/* Decode a p2pkh or p2sh into the ripemd160 hash */
bool ripemd160_from_base58(u8 *version, struct ripemd160 *rmd,
			   const char *base58, size_t base58_len);

#endif /* LIGHTNING_BROCOIN_BASE58_H */
