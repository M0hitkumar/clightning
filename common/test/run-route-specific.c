/* We can't seem to route the following:
 *
 * Expect route 03c173897878996287a8100469f954dd820fcd8941daed91c327f168f3329be0bf -> 0230ad0e74ea03976b28fda587bb75bdd357a1938af4424156a18265167f5e40ae -> 02ea622d5c8d6143f15ed3ce1d501dd0d3d09d3b1c83a44d0034949f8a9ab60f06
 *
 * getchannels:
 * {'channels': [{'active': True, 'short_id': '6990x2x1/1', 'fee_per_kw': 10, 'delay': 5, 'message_flags': 0, 'channel_flags': 1, 'destination': '0230ad0e74ea03976b28fda587bb75bdd357a1938af4424156a18265167f5e40ae', 'source': '02ea622d5c8d6143f15ed3ce1d501dd0d3d09d3b1c83a44d0034949f8a9ab60f06', 'last_update': 1504064344}, {'active': True, 'short_id': '6989x2x1/0', 'fee_per_kw': 10, 'delay': 5, 'message_flags': 0, 'channel_flags': 0, 'destination': '03c173897878996287a8100469f954dd820fcd8941daed91c327f168f3329be0bf', 'source': '0230ad0e74ea03976b28fda587bb75bdd357a1938af4424156a18265167f5e40ae', 'last_update': 1504064344}, {'active': True, 'short_id': '6990x2x1/0', 'fee_per_kw': 10, 'delay': 5, 'message_flags': 0, 'channel_flags': 0, 'destination': '02ea622d5c8d6143f15ed3ce1d501dd0d3d09d3b1c83a44d0034949f8a9ab60f06', 'source': '0230ad0e74ea03976b28fda587bb75bdd357a1938af4424156a18265167f5e40ae', 'last_update': 1504064344}, {'active': True, 'short_id': '6989x2x1/1', 'fee_per_kw': 10, 'delay': 5, 'message_flags': 0, 'channel_flags': 1, 'destination': '0230ad0e74ea03976b28fda587bb75bdd357a1938af4424156a18265167f5e40ae', 'source': '03c173897878996287a8100469f954dd820fcd8941daed91c327f168f3329be0bf', 'last_update': 1504064344}]}
 */
#include "config.h"
#include <assert.h>
#include <common/channel_type.h>
#include <common/dijkstra.h>
#include <common/gossmap.h>
#include <common/gossip_store.h>
#include <common/route.h>
#include <common/setup.h>
#include <common/type_to_string.h>
#include <common/utils.h>
#include <brocoin/chainparams.h>
#include <stdio.h>
#include <wire/peer_wiregen.h>
#include <unistd.h>

/* AUTOGENERATED MOCKS START */
/* Generated stub for fromwire_bigsize */
bigsize_t fromwire_bigsize(const u8 **cursor UNNEEDED, size_t *max UNNEEDED)
{ fprintf(stderr, "fromwire_bigsize called!\n"); abort(); }
/* Generated stub for fromwire_channel_id */
bool fromwire_channel_id(const u8 **cursor UNNEEDED, size_t *max UNNEEDED,
			 struct channel_id *channel_id UNNEEDED)
{ fprintf(stderr, "fromwire_channel_id called!\n"); abort(); }
/* Generated stub for fromwire_tlv */
bool fromwire_tlv(const u8 **cursor UNNEEDED, size_t *max UNNEEDED,
		  const struct tlv_record_type *types UNNEEDED, size_t num_types UNNEEDED,
		  void *record UNNEEDED, struct tlv_field **fields UNNEEDED,
		  const u64 *extra_types UNNEEDED, size_t *err_off UNNEEDED, u64 *err_type UNNEEDED)
{ fprintf(stderr, "fromwire_tlv called!\n"); abort(); }
/* Generated stub for fromwire_wireaddr */
bool fromwire_wireaddr(const u8 **cursor UNNEEDED, size_t *max UNNEEDED, struct wireaddr *addr UNNEEDED)
{ fprintf(stderr, "fromwire_wireaddr called!\n"); abort(); }
/* Generated stub for towire_bigsize */
void towire_bigsize(u8 **pptr UNNEEDED, const bigsize_t val UNNEEDED)
{ fprintf(stderr, "towire_bigsize called!\n"); abort(); }
/* Generated stub for towire_channel_id */
void towire_channel_id(u8 **pptr UNNEEDED, const struct channel_id *channel_id UNNEEDED)
{ fprintf(stderr, "towire_channel_id called!\n"); abort(); }
/* Generated stub for towire_tlv */
void towire_tlv(u8 **pptr UNNEEDED,
		const struct tlv_record_type *types UNNEEDED, size_t num_types UNNEEDED,
		const void *record UNNEEDED)
{ fprintf(stderr, "towire_tlv called!\n"); abort(); }
/* Generated stub for towire_wireaddr */
void towire_wireaddr(u8 **pptr UNNEEDED, const struct wireaddr *addr UNNEEDED)
{ fprintf(stderr, "towire_wireaddr called!\n"); abort(); }
/* AUTOGENERATED MOCKS END */

static void write_to_store(int store_fd, const u8 *msg)
{
	struct gossip_hdr hdr;

	hdr.len = cpu_to_be32(tal_count(msg));
	/* We don't actually check these! */
	hdr.crc = 0;
	hdr.timestamp = 0;
	assert(write(store_fd, &hdr, sizeof(hdr)) == sizeof(hdr));
	assert(write(store_fd, msg, tal_count(msg)) == tal_count(msg));
}

static void update_connection(int store_fd,
			      const struct node_id *from,
			      const struct node_id *to,
			      const char *shortid,
			      struct amount_mbro min,
			      struct amount_mbro max,
			      u32 base_fee, s32 proportional_fee,
			      u32 delay,
			      bool disable)
{
	struct short_channel_id scid;
	secp256k1_ecdsa_signature dummy_sig;
	u8 *msg;

	/* So valgrind doesn't complain */
	memset(&dummy_sig, 0, sizeof(dummy_sig));

	if (!short_channel_id_from_str(shortid, strlen(shortid), &scid))
		abort();

	msg = towire_channel_update_option_channel_htlc_max(tmpctx,
							    &dummy_sig,
							    &chainparams->genesis_blockhash,
							    &scid, 0,
							    ROUTING_OPT_HTLC_MAX_MBRO,
							    node_id_idx(from, to)
							    + (disable ? ROUTING_FLAGS_DISABLED : 0),
							    delay,
							    min,
							    base_fee,
							    proportional_fee,
							    max);

	write_to_store(store_fd, msg);
}

static void add_connection(int store_fd,
			   const struct node_id *from,
			   const struct node_id *to,
			   const char *shortid,
			   struct amount_mbro min,
			   struct amount_mbro max,
			   u32 base_fee, s32 proportional_fee,
			   u32 delay)
{
	struct short_channel_id scid;
	secp256k1_ecdsa_signature dummy_sig;
	struct secret not_a_secret;
	struct pubkey dummy_key;
	u8 *msg;
	const struct node_id *ids[2];

	/* So valgrind doesn't complain */
	memset(&dummy_sig, 0, sizeof(dummy_sig));
	memset(&not_a_secret, 1, sizeof(not_a_secret));
	pubkey_from_secret(&not_a_secret, &dummy_key);

	if (!short_channel_id_from_str(shortid, strlen(shortid), &scid))
		abort();

	if (node_id_cmp(from, to) > 0) {
		ids[0] = to;
		ids[1] = from;
	} else {
		ids[0] = from;
		ids[1] = to;
	}
	msg = towire_channel_announcement(tmpctx, &dummy_sig, &dummy_sig,
					  &dummy_sig, &dummy_sig,
					  /* features */ NULL,
					  &chainparams->genesis_blockhash,
					  &scid,
					  ids[0], ids[1],
					  &dummy_key, &dummy_key);
	write_to_store(store_fd, msg);

	update_connection(store_fd, from, to, shortid, min, max,
			  base_fee, proportional_fee,
			  delay, false);
}

static bool channel_is_between(const struct gossmap *gossmap,
			       const struct route_hop *route,
			       const struct gossmap_node *a,
			       const struct gossmap_node *b)
{
	const struct gossmap_chan *c = gossmap_find_chan(gossmap, &route->scid);

	if (c->half[route->direction].nodeidx
	    != gossmap_node_idx(gossmap, a))
		return false;
	if (c->half[!route->direction].nodeidx
	    != gossmap_node_idx(gossmap, b))
		return false;

	return true;
}

/* route_can_carry disregards unless *both* dirs are enabled, so we use
 * a simpler variant here */
static bool route_can_carry_unless_disabled(const struct gossmap *map,
					    const struct gossmap_chan *c,
					    int dir,
					    struct amount_mbro amount,
					    void *arg)
{
	if (!c->half[dir].enabled)
		return false;
	return route_can_carry_even_disabled(map, c, dir, amount, arg);
}

int main(int argc, char *argv[])
{
	struct node_id a, b, c, d;
	struct gossmap_node *a_node, *b_node, *c_node, *d_node;
	const struct dijkstra *dij;
	struct route_hop *route;
	int store_fd;
	struct gossmap *gossmap;
	const double riskfactor = 1.0;
	char gossip_version = GOSSIP_STORE_VERSION;
	char *gossipfilename;

	common_setup(argv[0]);
	node_id_from_hexstr("03c173897878996287a8100469f954dd820fcd8941daed91c327f168f3329be0bf",
			   strlen("03c173897878996287a8100469f954dd820fcd8941daed91c327f168f3329be0bf"),
			   &a);
	node_id_from_hexstr("0230ad0e74ea03976b28fda587bb75bdd357a1938af4424156a18265167f5e40ae",
			   strlen("0230ad0e74ea03976b28fda587bb75bdd357a1938af4424156a18265167f5e40ae"),
			   &b);
	node_id_from_hexstr("02ea622d5c8d6143f15ed3ce1d501dd0d3d09d3b1c83a44d0034949f8a9ab60f06",
			   strlen("02ea622d5c8d6143f15ed3ce1d501dd0d3d09d3b1c83a44d0034949f8a9ab60f06"),
			   &c);
	node_id_from_hexstr("02cca6c5c966fcf61d121e3a70e03a1cd9eeeea024b26ea666ce974d43b242e636",
			   strlen("02cca6c5c966fcf61d121e3a70e03a1cd9eeeea024b26ea666ce974d43b242e636"),
			   &d);

	chainparams = chainparams_for_network("regtest");

	store_fd = tmpdir_mkstemp(tmpctx, "run-route-specific-gossipstore.XXXXXX", &gossipfilename);
	assert(write(store_fd, &gossip_version, sizeof(gossip_version))
	       == sizeof(gossip_version));

	gossmap = gossmap_load(tmpctx, gossipfilename, NULL);

	/* [{'active': True, 'short_id': '6990:2:1/1', 'fee_per_kw': 10, 'delay': 5, 'message_flags': 0, 'channel_flags': 1, 'destination': '0230ad0e74ea03976b28fda587bb75bdd357a1938af4424156a18265167f5e40ae', 'source': '02ea622d5c8d6143f15ed3ce1d501dd0d3d09d3b1c83a44d0034949f8a9ab60f06', 'last_update': 1504064344}, */
	add_connection(store_fd, &c, &b, "6990x2x1",
		       AMOUNT_MBRO(0), AMOUNT_MBRO(1000),
		       0, 10, 5);

	/* {'active': True, 'short_id': '6989:2:1/0', 'fee_per_kw': 10, 'delay': 5, 'message_flags': 0, 'channel_flags': 0, 'destination': '03c173897878996287a8100469f954dd820fcd8941daed91c327f168f3329be0bf', 'source': '0230ad0e74ea03976b28fda587bb75bdd357a1938af4424156a18265167f5e40ae', 'last_update': 1504064344}, */
	add_connection(store_fd, &b, &a, "6989x2x1",
		       AMOUNT_MBRO(0), AMOUNT_MBRO(1000),
		       0, 10, 5);

	/* {'active': True, 'short_id': '6990:2:1/0', 'fee_per_kw': 10, 'delay': 5, 'message_flags': 0, 'channel_flags': 0, 'destination': '02ea622d5c8d6143f15ed3ce1d501dd0d3d09d3b1c83a44d0034949f8a9ab60f06', 'source': '0230ad0e74ea03976b28fda587bb75bdd357a1938af4424156a18265167f5e40ae', 'last_update': 1504064344}, */
	add_connection(store_fd, &b, &c, "6990x2x1",
		       AMOUNT_MBRO(100), AMOUNT_MBRO(1000),
		       0, 10, 5);

	/* {'active': True, 'short_id': '6989:2:1/1', 'fee_per_kw': 10, 'delay': 5, 'message_flags': 0, 'channel_flags': 1, 'destination': '0230ad0e74ea03976b28fda587bb75bdd357a1938af4424156a18265167f5e40ae', 'source': '03c173897878996287a8100469f954dd820fcd8941daed91c327f168f3329be0bf', 'last_update': 1504064344}]} */
	update_connection(store_fd, &a, &b, "6989x2x1",
			  AMOUNT_MBRO(0), AMOUNT_MBRO(1000),
			  0, 10, 5, false);

	assert(gossmap_refresh(gossmap, NULL));

	a_node = gossmap_find_node(gossmap, &a);
	b_node = gossmap_find_node(gossmap, &b);
	c_node = gossmap_find_node(gossmap, &c);

	dij = dijkstra(tmpctx, gossmap, c_node, AMOUNT_MBRO(1000), riskfactor,
		       route_can_carry_unless_disabled,
		       route_score_cheaper, NULL);
	route = route_from_dijkstra(tmpctx, gossmap, dij, a_node,
				    AMOUNT_MBRO(1000), 0);
	assert(route);
	assert(tal_count(route) == 2);
	assert(channel_is_between(gossmap, &route[0], a_node, b_node));
	assert(channel_is_between(gossmap, &route[1], b_node, c_node));

	/* We should not be able to find a route that exceeds our own capacity */
	dij = dijkstra(tmpctx, gossmap, c_node, AMOUNT_MBRO(1000001), riskfactor,
		       route_can_carry_unless_disabled,
		       route_score_cheaper, NULL);
	route = route_from_dijkstra(tmpctx, gossmap, dij, a_node,
				    AMOUNT_MBRO(1000), 0);
	assert(!route);

	/* Now test with a query that exceeds the channel capacity after adding
	 * some fees */
	dij = dijkstra(tmpctx, gossmap, c_node, AMOUNT_MBRO(999999), riskfactor,
		       route_can_carry_unless_disabled,
		       route_score_cheaper, NULL);
	route = route_from_dijkstra(tmpctx, gossmap, dij, a_node,
				    AMOUNT_MBRO(999999), 0);
	assert(!route);

	/* This should fail to return a route because it is smaller than these
	 * htlc_minimum_mbro on the last channel. */
	dij = dijkstra(tmpctx, gossmap, c_node, AMOUNT_MBRO(1), riskfactor,
		       route_can_carry_unless_disabled,
		       route_score_cheaper, NULL);
	route = route_from_dijkstra(tmpctx, gossmap, dij, a_node,
				    AMOUNT_MBRO(1), 0);
	assert(!route);

	/* {'active': True, 'short_id': '6990:2:1/0', 'fee_per_kw': 10, 'delay': 5, 'message_flags': 1, 'htlc_maximum_mbro': 500000, 'htlc_minimum_mbro': 100, 'channel_flags': 0, 'destination': '02cca6c5c966fcf61d121e3a70e03a1cd9eeeea024b26ea666ce974d43b242e636', 'source': '03c173897878996287a8100469f954dd820fcd8941daed91c327f168f3329be0bf', 'last_update': 1504064344}, */
	add_connection(store_fd, &a, &d, "6991x2x1",
		       AMOUNT_MBRO(100), AMOUNT_MBRO(499968), /* exact repr in fp16! */
		       0, 0, 5);
	assert(gossmap_refresh(gossmap, NULL));

	a_node = gossmap_find_node(gossmap, &a);
	b_node = gossmap_find_node(gossmap, &b);
	c_node = gossmap_find_node(gossmap, &c);
	d_node = gossmap_find_node(gossmap, &d);

	/* This should route correctly at the max_mbro level */
	dij = dijkstra(tmpctx, gossmap, d_node, AMOUNT_MBRO(499968), riskfactor,
		       route_can_carry_unless_disabled,
		       route_score_cheaper, NULL);
	route = route_from_dijkstra(tmpctx, gossmap, dij, a_node,
				    AMOUNT_MBRO(499968), 0);
	assert(route);

	/* This should fail to return a route because it's larger than the
	 * htlc_maximum_mbro on the last channel. */
	dij = dijkstra(tmpctx, gossmap, d_node, AMOUNT_MBRO(499968+1), riskfactor,
		       route_can_carry_unless_disabled,
		       route_score_cheaper, NULL);
	route = route_from_dijkstra(tmpctx, gossmap, dij, a_node,
				    AMOUNT_MBRO(499968+1), 0);
	assert(!route);

	common_shutdown();
	return 0;
}
