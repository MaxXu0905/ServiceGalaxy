include $(SGROOT)/etc/env_public.mk
GP_HEADERS = -I$(GPHOME)/include
GP_LIB = -L$(GPHOME)/lib -lpq
CFLAGS = $(PCFLAGS) $(GP_HEADERS)
LDFLAGS = $(PLDFLAGS) -lgp -lsg -lsqlgp $(GP_LIB)
SELECT_OBJS = src/auto_select.o src/select_main.o
INSERT_OBJS = src/auto_insert.o src/insert_main.o
UPDATE_OBJS = src/auto_update.o src/update_main.o
DELETE_OBJS = src/auto_delete.o src/delete_main.o
DYNAMIC_OBJS = src/dynamic_main.o

BINS = run_select run_insert run_update run_delete run_dynamic 
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

code_gen: code_gen.sh
	touch code_gen
	sh code_gen.sh

include $(SGROOT)/etc/env_suffix.mk
