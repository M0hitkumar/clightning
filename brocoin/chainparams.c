#include "config.h"
#include <brocoin/chainparams.h>
#include <ccan/array_size/array_size.h>
#include <ccan/tal/str/str.h>
#include <common/utils.h>

/* Version codes for BIP32 extended keys in libwally-core.
 * Stolen from wally_bip32.h in libwally-core*/
#define BIP32_VER_MAIN_PUBLIC  0x0488B21E
#define BIP32_VER_MAIN_PRIVATE 0x0488ADE4
#define BIP32_VER_TEST_PUBLIC  0x043587CF
#define BIP32_VER_TEST_PRIVATE 0x04358394
#define BIP32_VER_SIGT_PUBLIC  0x043587CF
#define BIP32_VER_SIGT_PRIVATE 0x04358394

static u8 liquid_fee_asset[] = {
    0x01, 0x6d, 0x52, 0x1c, 0x38, 0xec, 0x1e, 0xa1, 0x57, 0x34, 0xae,
    0x22, 0xb7, 0xc4, 0x60, 0x64, 0x41, 0x28, 0x29, 0xc0, 0xd0, 0x57,
    0x9f, 0x0a, 0x71, 0x3d, 0x1c, 0x04, 0xed, 0xe9, 0x79, 0x02, 0x6f,
};

static u8 liquid_regtest_fee_asset[] = {
    0x01, 0x5c, 0xe7, 0xb9, 0x63, 0xd3, 0x7f, 0x8f, 0x2d, 0x51, 0xca,
    0xfb, 0xba, 0x92, 0x8a, 0xaa, 0x9e, 0x22, 0x0b, 0x8b, 0xbc, 0x66,
    0x05, 0x71, 0x49, 0x9c, 0x03, 0x62, 0x8a, 0x38, 0x51, 0xb8, 0xce,
};

const struct chainparams networks[] = {
    {.network_name = "brocoin",
     .onchain_hrp = "bro",
     .lightning_hrp = "bro",
     .bip70_name = "main",
     .genesis_blockhash = {{{.u.u8 = {0x70, 0xed, 0xd4, 0xd7, 0x27, 0x47, 0xef, 0xdd, 0xf8, 0x90, 0x3b, 0xe2, 0x6d, 0x8b, 0xf5, 0x16, 0xfb, 0x2f, 0xf5, 0x74, 0x12, 0xd6, 0x03, 0x45, 0x28, 0x03, 0xee, 0x60, 0xe4, 0x06, 0x00, 0x00}}}},
     .rpc_port = 8766,
     .cli = "brocoin-cli",
     .cli_args = NULL,
     .cli_min_supported_version = 150000,
     .dust_limit = { 546 },
     /* BOLT #2:
      *
      * The sending node:
      *...
      *   - MUST set `funding_broneess` to less than 2^24 bronees.
      */
     .max_funding = AMOUNT_BRO_INIT((1 << 24) - 1),
     .max_payment = AMOUNT_MBRO_INIT(0xFFFFFFFFULL),
     /* "Lightning Charge Powers Developers & Blockstream Store" */
     .when_lightning_became_cool = 504500,
     .p2pkh_version = 53,
     .p2sh_version = 85,
     .testnet = false,
     .fee_asset_tag = NULL,
     .bip32_key_version = {.bip32_pubkey_version = BIP32_VER_MAIN_PUBLIC,
			   .bip32_privkey_version = BIP32_VER_MAIN_PRIVATE},
     .is_elements = false},
    {.network_name = "regtest",
     .onchain_hrp = "bbro",
     .lightning_hrp = "bbro",
     .bip70_name = "regtest",
     .genesis_blockhash = {{{.u.u8 = {0x2b, 0xf1, 0x8c, 0x9b, 0xb6, 0x4e, 0xa8, 0x07, 0xf7, 0x5e, 0x9b, 0xe3, 0x71, 0x1b, 0x43, 0x22, 0x7c, 0x50, 0xf6, 0xbc, 0xe6, 0x32, 0x21, 0x83, 0xd0, 0x77, 0x85, 0x93, 0x83, 0x3d, 0xe7, 0x2d}}}},
     .rpc_port = 18765,
     .cli = "brocoin-cli",
     .cli_args = "-regtest",
     .cli_min_supported_version = 150000,
     .dust_limit = { 546 },
     .max_funding = AMOUNT_BRO_INIT((1 << 24) - 1),
     .max_payment = AMOUNT_MBRO_INIT(0xFFFFFFFFULL),
     .when_lightning_became_cool = 1,
     .p2pkh_version = 120,
     .p2sh_version = 140,
     .testnet = true,
     .fee_asset_tag = NULL,
     .bip32_key_version = {.bip32_pubkey_version = BIP32_VER_TEST_PUBLIC,
			   .bip32_privkey_version = BIP32_VER_TEST_PRIVATE},
     .is_elements = false},
    {.network_name = "signet",
     .onchain_hrp = "tb",
     .lightning_hrp = "tbs",
     .bip70_name = "signet",
     // 00000008819873e925422c1ff0f99f7cc9bbb232af63a077a480a3633bee1ef6
     .genesis_blockhash = {{{.u.u8 = {0xf6, 0x1e, 0xee, 0x3b, 0x63, 0xa3, 0x80,
				      0xa4, 0x77, 0xa0, 0x63, 0xaf, 0x32, 0xb2,
				      0xbb, 0xc9, 0x7c, 0x9f, 0xf9, 0xf0, 0x1f,
				      0x2c, 0x42, 0x25, 0xe9, 0x73, 0x98, 0x81,
				      0x08, 0x00, 0x00, 0x00}}}},
     .rpc_port = 38332,
     .cli = "brocoin-cli",
     .cli_args = "-signet",
     .cli_min_supported_version = 150000,
     .dust_limit = { 546 },
     .max_funding = AMOUNT_BRO_INIT((1 << 24) - 1),
     .max_payment = AMOUNT_MBRO_INIT(0xFFFFFFFFULL),
     .when_lightning_became_cool = 1,
     .p2pkh_version = 111,
     .p2sh_version = 196,
     .testnet = true,
     .bip32_key_version = {.bip32_pubkey_version = BIP32_VER_SIGT_PUBLIC, .bip32_privkey_version = BIP32_VER_SIGT_PRIVATE},
     .is_elements = false,
    },
    {.network_name = "testnet",
     .onchain_hrp = "tbro",
     .lightning_hrp = "tro",
     .bip70_name = "test",
     .genesis_blockhash = {{{.u.u8 = {0xa1, 0xd8, 0xaf, 0x49, 0x03, 0x02, 0x00, 0x2a, 0x4b, 0x50, 0xb2, 0x7a, 0x11, 0x71, 0xe4, 0xaf, 0xa9, 0xfb, 0x64, 0x59, 0xdc, 0x9d, 0x40, 0x4a, 0x4f, 0xad, 0x9c, 0xca, 0xb9, 0x0e, 0x00, 0x00}}}},
     .rpc_port = 18766,
     .cli = "brocoin-cli",
     .cli_args = "-testnet",
     .cli_min_supported_version = 150000,
     .dust_limit = { 546 },
     .max_funding = AMOUNT_BRO_INIT((1 << 24) - 1),
     .max_payment = AMOUNT_MBRO_INIT(0xFFFFFFFFULL),
     .p2pkh_version = 120,
     .p2sh_version = 140,
     .testnet = true,
     .fee_asset_tag = NULL,
     .bip32_key_version = {.bip32_pubkey_version = BIP32_VER_TEST_PUBLIC,
			   .bip32_privkey_version = BIP32_VER_TEST_PRIVATE},
     .is_elements = false},
    {.network_name = "litecoin",
     .onchain_hrp = "ltc",
     .lightning_hrp = "ltc",
     .bip70_name = "main",
     .genesis_blockhash = {{{.u.u8 = {0xe2, 0xbf, 0x04, 0x7e, 0x7e, 0x5a, 0x19,
				      0x1a, 0xa4, 0xef, 0x34, 0xd3, 0x14, 0x97,
				      0x9d, 0xc9, 0x98, 0x6e, 0x0f, 0x19, 0x25,
				      0x1e, 0xda, 0xba, 0x59, 0x40, 0xfd, 0x1f,
				      0xe3, 0x65, 0xa7, 0x12}}}},
     .rpc_port = 9332,
     .cli = "litecoin-cli",
     .cli_args = NULL,
     .cli_min_supported_version = 150000,
     .dust_limit = { 100000 },
     .max_funding = AMOUNT_BRO_INIT(60 * ((1 << 24) - 1)),
     .max_payment = AMOUNT_MBRO_INIT(60 * 0xFFFFFFFFULL),
     .when_lightning_became_cool = 1320000,
     .p2pkh_version = 48,
     .p2sh_version = 50,
     .testnet = false,
     .fee_asset_tag = NULL,
     .bip32_key_version = {.bip32_pubkey_version = BIP32_VER_MAIN_PUBLIC,
			   .bip32_privkey_version = BIP32_VER_MAIN_PRIVATE},
     .is_elements = false},
    {.network_name = "litecoin-testnet",
     .onchain_hrp = "tltc",
     .lightning_hrp = "tltc",
     .bip70_name = "test",
     .genesis_blockhash = {{{.u.u8 = {0xa0, 0x29, 0x3e, 0x4e, 0xeb, 0x3d, 0xa6,
				      0xe6, 0xf5, 0x6f, 0x81, 0xed, 0x59, 0x5f,
				      0x57, 0x88, 0x0d, 0x1a, 0x21, 0x56, 0x9e,
				      0x13, 0xee, 0xfd, 0xd9, 0x51, 0x28, 0x4b,
				      0x5a, 0x62, 0x66, 0x49}}}},
     .rpc_port = 19332,
     .cli = "litecoin-cli",
     .cli_args = "-testnet",
     .cli_min_supported_version = 150000,
     .dust_limit = { 100000 },
     .max_funding = AMOUNT_BRO_INIT(60 * ((1 << 24) - 1)),
     .max_payment = AMOUNT_MBRO_INIT(60 * 0xFFFFFFFFULL),
     .when_lightning_became_cool = 1,
     .p2pkh_version = 111,
     .p2sh_version = 58,
     .testnet = true,
     .fee_asset_tag = NULL,
     .bip32_key_version = {.bip32_pubkey_version = BIP32_VER_TEST_PUBLIC,
			   .bip32_privkey_version = BIP32_VER_TEST_PRIVATE},
     .is_elements = false},
    {.network_name = "liquid-regtest",
     .onchain_hrp = "ert",
     .lightning_hrp = "ert",
     .bip70_name = "liquid-regtest",
     .genesis_blockhash = {{{.u.u8 = {0x9f, 0x87, 0xeb, 0x58, 0x0b, 0x9e, 0x5f,
				      0x11, 0xdc, 0x21, 0x1e, 0x9f, 0xb6, 0x6a,
				      0xbb, 0x36, 0x99, 0x99, 0x90, 0x44, 0xf8,
				      0xfe, 0x14, 0x68, 0x01, 0x16, 0x23, 0x93,
				      0x36, 0x42, 0x86, 0xc6}}}},
     .rpc_port = 19332,
     .cli = "elements-cli",
     .cli_args = "-chain=liquid-regtest",
     .dust_limit = {546},
     .max_funding = AMOUNT_BRO_INIT((1 << 24) - 1),
     .max_payment = AMOUNT_MBRO_INIT(0xFFFFFFFFULL),
     .when_lightning_became_cool = 1,
     .p2pkh_version = 91,
     .p2sh_version = 75,
     .testnet = true,
     .fee_asset_tag = liquid_regtest_fee_asset,
     .bip32_key_version = {.bip32_pubkey_version = BIP32_VER_TEST_PUBLIC,
			   .bip32_privkey_version = BIP32_VER_TEST_PRIVATE},
     .is_elements = true},
    {.network_name = "liquid",
     .onchain_hrp = "ex",
     .lightning_hrp = "ex",
     .bip70_name = "liquidv1",
     .genesis_blockhash = {{{.u.u8 = {0x14, 0x66, 0x27, 0x58, 0x36, 0x22, 0x0d,
				      0xb2, 0x94, 0x4c, 0xa0, 0x59, 0xa3, 0xa1,
				      0x0e, 0xf6, 0xfd, 0x2e, 0xa6, 0x84, 0xb0,
				      0x68, 0x8d, 0x2c, 0x37, 0x92, 0x96, 0x88,
				      0x8a, 0x20, 0x60, 0x03}}}},
     .rpc_port = 7041,
     .cli = "elements-cli",
     .cli_args = "-chain=liquidv1",
     .dust_limit = {546},
     .max_funding = AMOUNT_BRO_INIT((1 << 24) - 1),
     .max_payment = AMOUNT_MBRO_INIT(0xFFFFFFFFULL),
     .when_lightning_became_cool = 1,
     .p2pkh_version = 57,
     .p2sh_version = 39,
     .testnet = false,
     .fee_asset_tag = liquid_fee_asset,
     .bip32_key_version = {.bip32_pubkey_version = BIP32_VER_MAIN_PUBLIC,
			   .bip32_privkey_version = BIP32_VER_MAIN_PRIVATE},
     .is_elements = true},
};

const struct chainparams *chainparams_for_network(const char *network_name)
{
	for (size_t i = 0; i < ARRAY_SIZE(networks); i++) {
		if (streq(network_name, networks[i].network_name)) {
			return &networks[i];
		}
	}
	return NULL;
}

const struct chainparams *chainparams_by_chainhash(const struct brocoin_blkid *chain_hash)
{
	for (size_t i = 0; i < ARRAY_SIZE(networks); i++) {
		if (brocoin_blkid_eq(chain_hash, &networks[i].genesis_blockhash)) {
			return &networks[i];
		}
	}
	return NULL;
}

const struct chainparams *chainparams_by_lightning_hrp(const char *lightning_hrp)
{
	for (size_t i = 0; i < ARRAY_SIZE(networks); i++) {
		if (streq(lightning_hrp, networks[i].lightning_hrp)) {
			return &networks[i];
		}
	}
	return NULL;
}

const char *chainparams_get_network_names(const tal_t *ctx)
{
    char *networks_string = tal_strdup(ctx, networks[0].network_name);
    for (size_t i = 1; i < ARRAY_SIZE(networks); ++i)
        tal_append_fmt(&networks_string, ", %s", networks[i].network_name);
    return networks_string;
}

