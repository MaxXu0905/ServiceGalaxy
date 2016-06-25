include $(SGROOT)/etc/env_public.mk
CFLAGS = $(PCFLAGS) $(ORACLE_HEADERS) -I$(SQLITE_ROOT)/include
LDFLAGS = $(PLDFLAGS) -lgp -lsg -L$(BOOST_ROOT)/stage/lib -lboost_system -lboost_thread \
    -lboost_program_options -lboost_filesystem -lboost_chrono -L$(SQLITE_ROOT)/lib -lsqlite3
BINS = build_meta dbc_server dbc sdc dbc_admin dbc_sync dbc_dump dbc_insert recompile_dbc.sh
ifdef ORACLE_HOME
BINS := $(BINS) db2dbc
endif
LIBS = libdbc.$(SO) libsqldbc.$(SO)
SKIPS = include/t_sys_index_lock.h src/t_sys_index_lock.cpp
TMPS = recompile_dbc.sh
SRCS = src/dbc_perf.cpp
ETCS = dbc.xsd sdc.xsd so_static.exp makefile.meta
TARGET = $(LIBS) $(BINS)

OBJS = src/dbc_struct.o src/dbc_manager.o src/dbcp_ctx.o src/dbct_ctx.o \
	src/dbc_config.o src/dbc_ue.o src/dbc_te.o src/dbc_ie.o src/dbc_rte.o \
	src/dbc_redo.o src/dbc_sqlite.o src/redo_rbtree.o src/inode_rbtree.o \
	src/inode_list.o src/endian_convert.o src/dbc_function.o src/dbc_se.o \
	src/dbc_stat.o src/dbc_api.o src/sdc_struct.o src/sdc_api.o src/dbc_int_function.o \
	src/sdc_config.o src/dbc_lock.o
SQLDBC_OBJS = src/dbc_control.o
DBC_OBJS = src/dbc_server.o src/dbc_msvr.o src/dbc_msvc.o
SDC_OBJS = src/sdc_server.o src/sdc_svc.o src/sdc_sqlsvc.o
SVR_OBJS = src/server_main.o src/dbc_server.o
BUILD_OBJS = src/build_meta.o src/meta_main.o
ADMIN_OBJS = src/dbc_admin.o src/dbc_server.o src/cmd_dparser.o src/common_dparser.o \
	src/bb_dparser.o src/prt_dparser.o src/sql_dparser.o
SYNC_OBJS = src/dbc_sync.o src/sync_main.o
DUMP_OBJS = src/dbc_dump.o src/dump_main.o
DB2DBC_OBJS = src/db2dbc.o
INSERT_OBJS = src/dbc_insert.o

all: $(TARGET) install

dbc_server:$(SVR_OBJS) libdbc.$(SO)
	$(CC) $(CFLAGS) -o $@ $(SVR_OBJS) -L. -ldbc -lsqlcommon $(LDFLAGS)

dbc:$(DBC_OBJS) libdbc.$(SO)
	sgcompile --server --output=$@ --first="-L. -ldbc -lsqlcommon -L$(SQLITE_ROOT)/lib -lsqlite3 $(CFLAGS) $(DBC_OBJS)" --server-class=dbc_msvr --operation=DBC_METASVC:dbc_msvc --system

sdc:$(SDC_OBJS) libdbc.$(SO) libsqldbc.$(SO)
	sgcompile --server --output=$@ --first="-L. -ldbc -lsqldbc $(CFLAGS) $(SDC_OBJS)" --server-class=sdc_server --operation=.SDC_SVC:sdc_svc --operation=.SDC_SQLSVC:sdc_sqlsvc --system

libdbc.$(SO):$(OBJS)
	$(CC) $(CFLAGS) $(SOFLAGS) -o $@ $(OBJS) $(LDFLAGS)

libsqldbc.$(SO):$(SQLDBC_OBJS) libdbc.$(SO)
	$(CC) $(CFLAGS) $(SOFLAGS) -o $@ $(SQLDBC_OBJS) -L. -ldbc $(LDFLAGS) -lsqlcommon -lpubapp

build_meta:$(BUILD_OBJS) libdbc.$(SO)
	$(CC) $(CFLAGS) -o $@ $(BUILD_OBJS) -L. -ldbc $(LDFLAGS)

dbc_admin:$(ADMIN_OBJS) libdbc.$(SO) libsqldbc.$(SO)
	$(CC) $(CFLAGS) -o $@ $(ADMIN_OBJS) -L. -ldbc -lsqlcommon -lsqldbc -lpubapp $(LDFLAGS)

dbc_sync:$(SYNC_OBJS) libdbc.$(SO)
	$(CC) $(CFLAGS) -o $@ $(SYNC_OBJS) -L. -ldbc -lsqlcommon -lpubapp $(LDFLAGS)

dbc_dump:$(DUMP_OBJS) libdbc.$(SO)
	$(CC) $(CFLAGS) -o $@ $(DUMP_OBJS) -L. -ldbc -lsqlcommon -lpubapp $(LDFLAGS)

db2dbc:$(DB2DBC_OBJS) libdbc.$(SO)
	$(CC) $(CFLAGS) -o $@ $(DB2DBC_OBJS) -L. -ldbc -lsqlcommon -lpubapp -lsqloracle $(LDFLAGS)

dbc_insert:$(INSERT_OBJS) libdbc.$(SO) libsqldbc.$(SO)
	$(CC) $(CFLAGS) -o $@ $(INSERT_OBJS) -L. -ldbc -lsqlcommon -lsqldbc -lpubapp $(LDFLAGS)

recompile_dbc.sh:
	echo "export MODE=$(MODE)" > $@
	echo "export DEBUG=$(DEBUG)" >> $@
	echo "export SGROOT=$${SGROOT:-$(SGROOT)}" >> $@
	echo "export BOOST_ROOT=$${BOOST_ROOT:-$(BOOST_ROOT)}" >> $@
	echo "export ZLIB_ROOT=$${ZLIB_ROOT:-$(ZLIB_ROOT)}" >> $@
	echo "export SQLITE_ROOT=$${SQLITE_ROOT:-$(SQLITE_ROOT)}" >> $@
	echo "export LIBEVENT_ROOT=$${LIBEVENT_ROOT:-$(LIBEVENT_ROOT)}" >> $@
	echo "export THRIFT_ROOT=$${THRIFT_ROOT:-$(THRIFT_ROOT)}" >> $@
	echo "export PERF_TABLE=$${PERF_TABLE:-$(PERF_TABLE)}" >> $@
	echo "" >> $@
	echo "if [ $$""# -ne 1 ]" >> $@
	echo "then" >> $@
	echo "	echo \"Usage: \" $$""0 \"all|clean\"" >> $@
	echo "	exit 1" >> $@
	echo "fi" >> $@
	echo "" >> $@
	echo "cd $$""SGROOT" >> $@
	echo "make -j -f etc/makefile.meta $$""1" >> $@

include $(SGROOT)/etc/env_suffix.mk
