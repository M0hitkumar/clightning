# Included for one dir up.

BROCOIN_SRC :=					\
	brocoin/base58.c			\
	brocoin/block.c				\
	brocoin/chainparams.c			\
	brocoin/feerate.c			\
	brocoin/locktime.c			\
	brocoin/preimage.c			\
	brocoin/privkey.c			\
	brocoin/psbt.c				\
	brocoin/pubkey.c			\
	brocoin/script.c			\
	brocoin/shadouble.c			\
	brocoin/short_channel_id.c		\
	brocoin/signature.c			\
	brocoin/tx.c				\
	brocoin/tx_parts.c			\
	brocoin/varint.c

BROCOIN_OBJS := $(BROCOIN_SRC:.c=.o)

BROCOIN_HEADERS := brocoin/address.h		\
	brocoin/base58.h			\
	brocoin/block.h				\
	brocoin/chainparams.h			\
	brocoin/feerate.h			\
	brocoin/locktime.h			\
	brocoin/preimage.h			\
	brocoin/privkey.h			\
	brocoin/psbt.h				\
	brocoin/pubkey.h			\
	brocoin/script.h			\
	brocoin/shadouble.h			\
	brocoin/short_channel_id.h		\
	brocoin/signature.h			\
	brocoin/tx.h				\
	brocoin/tx_parts.h			\
	brocoin/varint.h

# Brocoin objects depends on brocoin/ external/ and ccan
$(BROCOIN_OBJS): $(CCAN_HEADERS) $(BROCOIN_HEADERS) $(EXTERNAL_HEADERS)

check-makefile: check-brocoin-makefile

check-brocoin-makefile:
	@if [ "`echo brocoin/*.h`" != "$(BROCOIN_HEADERS)" ]; then echo BROCOIN_HEADERS incorrect; exit 1; fi

check-whitespace: check-whitespace/brocoin/Makefile

clean: brocoin-clean

brocoin-clean:
	$(RM) $(BROCOIN_OBJS) $(BROCOIN_TEST_PROGRAMS) $(BROCOIN_TEST_OBJS)

include brocoin/test/Makefile
