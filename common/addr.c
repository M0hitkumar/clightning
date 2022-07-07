#include "config.h"
#include <brocoin/address.h>
#include <brocoin/base58.h>
#include <brocoin/script.h>
#include <common/addr.h>
#include <common/bech32.h>

char *encode_scriptpubkey_to_addr(const tal_t *ctx,
				  const struct chainparams *chainparams,
				  const u8 *scriptPubkey)
{
	char *out;
	size_t scriptLen = tal_bytelen(scriptPubkey);
	struct brocoin_address pkh;
	struct ripemd160 sh;

	if (is_p2pkh(scriptPubkey, &pkh))
		return brocoin_to_base58(ctx, chainparams, &pkh);

	if (is_p2sh(scriptPubkey, &sh))
		return p2sh_to_base58(ctx, chainparams, &sh);

	out = tal_arr(ctx, char, 73 + strlen(chainparams->onchain_hrp));
	if (!segwit_addr_encode(out, chainparams->onchain_hrp, 0,
				scriptPubkey + 2, scriptLen - 2))
		return tal_free(out);

	return out;
}
