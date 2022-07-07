BROCOIN_TEST_SRC := $(wildcard brocoin/test/run-*.c)
BROCOIN_TEST_OBJS := $(BROCOIN_TEST_SRC:.c=.o)
BROCOIN_TEST_PROGRAMS := $(BROCOIN_TEST_OBJS:.o=)

BROCOIN_TEST_COMMON_OBJS := common/utils.o common/setup.o common/autodata.o

$(BROCOIN_TEST_PROGRAMS): $(CCAN_OBJS) $(BROCOIN_TEST_COMMON_OBJS) brocoin/chainparams.o
$(BROCOIN_TEST_OBJS): $(CCAN_HEADERS) $(BROCOIN_HEADERS) $(BROCOIN_SRC)

ALL_TEST_PROGRAMS += $(BROCOIN_TEST_PROGRAMS)
ALL_C_SOURCES += $(BROCOIN_TEST_PROGRAMS:=.c)

# This needs to know what level of optimization we're using.
brocoin/test/run-secret_eq_consttime.o: CFLAGS += -DCOPTFLAGS="\"${COPTFLAGS}\""

check-units: $(BROCOIN_TEST_PROGRAMS:%=unittest/%)
