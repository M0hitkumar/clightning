#ifndef LIGHTNING_LIGHTNINGD_BROCOIND_H
#define LIGHTNING_LIGHTNINGD_BROCOIND_H
#include "config.h"
#include <brocoin/chainparams.h>
#include <brocoin/tx.h>
#include <ccan/list/list.h>
#include <ccan/strmap/strmap.h>

struct brocoin_blkid;
struct brocoin_tx_output;
struct block;
struct lightningd;
struct ripemd160;
struct brocoin_tx;
struct brocoin_block;

struct brocoind {
	/* Where to do logging. */
	struct log *log;

	/* Main lightningd structure */
	struct lightningd *ld;

	/* Is our Brocoin backend synced?  If not, we retry. */
	bool synced;

	/* Ignore results, we're shutting down. */
	bool shutdown;

	/* Timer if we're waiting for it to warm up. */
	struct oneshot *checkchain_timer;

	struct list_head pending_getfilteredblock;

	/* Map each method to a plugin, so we can have multiple plugins
	 * handling different functionalities. */
	STRMAP(struct plugin *) pluginsmap;
};

/* A single outpoint in a filtered block */
struct filteredblock_outpoint {
	struct brocoin_outpoint outpoint;
	u32 txindex;
	const u8 *scriptPubKey;
	struct amount_bro amount;
};

/* A struct representing a block with most of the parts filtered out. */
struct filteredblock {
	struct brocoin_blkid id;
	u32 height;
	struct brocoin_blkid prev_hash;
	struct filteredblock_outpoint **outpoints;
};

struct brocoind *new_brocoind(const tal_t *ctx,
			      struct lightningd *ld,
			      struct log *log);

void brocoind_estimate_fees_(struct brocoind *brocoind,
			     size_t num_estimates,
			     void (*cb)(struct brocoind *brocoind,
					const u32 bronees_per_kw[], void *),
			     void *arg);

#define brocoind_estimate_fees(brocoind_, num, cb, arg) \
	brocoind_estimate_fees_((brocoind_), (num), \
				typesafe_cb_preargs(void, void *,	\
						    (cb), (arg),	\
						    struct brocoind *,	\
						    const u32 *),	\
				(arg))

void brocoind_sendrawtx_ahf_(struct brocoind *brocoind,
			     const char *hextx,
			     bool allowhighfees,
			     void (*cb)(struct brocoind *brocoind,
					bool success, const char *msg, void *),
			     void *arg);
#define brocoind_sendrawtx_ahf(brocoind_, hextx, allowhighfees, cb, arg)\
	brocoind_sendrawtx_ahf_((brocoind_), (hextx),			\
				(allowhighfees),			\
				typesafe_cb_preargs(void, void *,	\
						    (cb), (arg),	\
						    struct brocoind *,	\
						    bool, const char *),\
				(arg))

void brocoind_sendrawtx_(struct brocoind *brocoind,
			 const char *hextx,
			 void (*cb)(struct brocoind *brocoind,
				    bool success, const char *msg, void *),
			 void *arg);

#define brocoind_sendrawtx(brocoind_, hextx, cb, arg)			\
	brocoind_sendrawtx_((brocoind_), (hextx),			\
			    typesafe_cb_preargs(void, void *,		\
						(cb), (arg),		\
						struct brocoind *,	\
						bool, const char *),	\
			    (arg))

void brocoind_getfilteredblock_(struct brocoind *brocoind, u32 height,
				void (*cb)(struct brocoind *brocoind,
					   const struct filteredblock *fb,
					   void *arg),
				void *arg);
#define brocoind_getfilteredblock(brocoind_, height, cb, arg)		\
	brocoind_getfilteredblock_((brocoind_),				\
				   (height),				\
				   typesafe_cb_preargs(void, void *,	\
						       (cb), (arg),	\
						       struct brocoind *, \
						       const struct filteredblock *), \
				   (arg))

void brocoind_getchaininfo_(struct brocoind *brocoind,
			    const bool first_call,
			    void (*cb)(struct brocoind *brocoind,
				       const char *chain,
				       u32 headercount,
				       u32 blockcount,
				       const bool ibd,
				       const bool first_call, void *),
			    void *cb_arg);
#define brocoind_getchaininfo(brocoind_, first_call_, cb, arg)		   \
	brocoind_getchaininfo_((brocoind_), (first_call_),		   \
			      typesafe_cb_preargs(void, void *,		   \
						  (cb), (arg),		   \
						  struct brocoind *,	   \
						  const char *, u32, u32,  \
						  const bool, const bool), \
			      (arg))

void brocoind_getrawblockbyheight_(struct brocoind *brocoind,
				   u32 height,
				   void (*cb)(struct brocoind *brocoind,
					      struct brocoin_blkid *blkid,
					      struct brocoin_block *blk,
					      void *arg),
				   void *arg);
#define brocoind_getrawblockbyheight(brocoind_, height_, cb, arg)		\
	brocoind_getrawblockbyheight_((brocoind_), (height_),			\
				      typesafe_cb_preargs(void, void *,		\
							  (cb), (arg),		\
							  struct brocoind *,	\
							  struct brocoin_blkid *, \
							  struct brocoin_block *),\
				      (arg))

void brocoind_getutxout_(struct brocoind *brocoind,
			 const struct brocoin_outpoint *outpoint,
			 void (*cb)(struct brocoind *brocoind,
				    const struct brocoin_tx_output *txout,
				    void *arg),
			 void *arg);
#define brocoind_getutxout(brocoind_, outpoint_, cb, arg)		\
	brocoind_getutxout_((brocoind_), (outpoint_),			\
			    typesafe_cb_preargs(void, void *,		\
					        (cb), (arg),		\
					        struct brocoind *,	\
					        struct brocoin_tx_output *),\
			    (arg))

void brocoind_getclientversion(struct brocoind *brocoind);

void brocoind_check_commands(struct brocoind *brocoind);

#endif /* LIGHTNING_LIGHTNINGD_BROCOIND_H */
