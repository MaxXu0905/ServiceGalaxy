include $(SGROOT)/etc/env_public.mk
CFLAGS = $(PCFLAGS) $(ORACLE_HEADERS) -I$(SQLITE_ROOT)/include
LDFLAGS = $(PLDFLAGS) $(ORACLE_LIB) -lgp -lsg -lsqldbc
SELECT_OBJS = src/auto_select.o src/select_main.o
INSERT_OBJS = src/auto_insert.o src/insert_main.o
UPDATE_OBJS = src/auto_update.o src/update_main.o
DELETE_OBJS = src/auto_delete.o src/delete_main.o
DYNAMIC_OBJS = src/dynamic_main.o
DBC_COBJS = src/dbc_example.o
SDC_COBJS = src/sdc_example.o

BINS = run_select run_insert run_update run_delete run_dynamic dbc_clt sdc_clt
SAMPLES = dbc
TARGET = code_gen $(BINS)

all:$(TARGET) install

run_select: $(SELECT_OBJS) code_gen
	$(CC) $(CFLAGS) -o $@ $(SELECT_OBJS) $(LDFLAGS)

run_insert: $(INSERT_OBJS) code_gen
	$(CC) $(CFLAGS) -o $@ $(INSERT_OBJS) $(LDFLAGS)

run_update: $(UPDATE_OBJS) code_gen
	$(CC) $(CFLAGS) -o $@ $(UPDATE_OBJS) $(LDFLAGS)

run_delete: $(DELETE_OBJS) code_gen
	$(CC) $(CFLAGS) -o $@ $(DELETE_OBJS) $(LDFLAGS)

run_dynamic: $(DYNAMIC_OBJS) code_gen
	$(CC) $(CFLAGS) -o $@ $(DYNAMIC_OBJS) $(LDFLAGS)

dbc_clt: $(DBC_COBJS) code_gen
	$(CC) $(CFLAGS) -o $@ $(DBC_COBJS) $(LDFLAGS)

sdc_clt:$(SDC_COBJS) code_gen
	sgcompile --client --output=$@ --first="$(CFLAGS) $(SDC_COBJS)" --last="$(LDFLAGS) -ldbcclient -lsdcsearch"

code_gen: code_gen.sh
	touch code_gen
	sh code_gen.sh

include $(SGROOT)/etc/env_suffix.mk
