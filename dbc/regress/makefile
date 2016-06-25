include $(SGROOT)/etc/env_public.mk
CFLAGS = $(PCFLAGS) $(ORACLE_HEADERS) $(ORACLE_FLAGS)
LDFLAGS = $(PLDFLAGS) $(ORACLE_LIB) $(OCCI_LIB) -L. -lgp -lsg -lsqloracle -ldbc -ldbcclient -ldbcsearch
BINS = simpapp dbc_test
TARGET = $(BINS)

OBJS = src/test_ctx.o src/test_manager.o src/test_thread.o src/test_main.o

all:$(TARGET) install

simpapp:src/simpapp.o
	$(CC) $(CFLAGS) -o $@ src/simpapp.o $(LDFLAGS)
ifndef DEBUG
	-$(STRIP) $@
endif

dbc_test:$(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
ifndef DEBUG
	-$(STRIP) $@
endif

include $(SGROOT)/etc/env_suffix.mk
