include $(SGROOT)/etc/env_public.mk
ALTIBASE_HEADERS = -I$(ALTIBASE_HOME)/include
ALTIBASE_LIB = -L$(ALTIBASE_HOME)/lib -laltibase
GP_HEADERS = -I$(GPHOME)/include
GP_LIB = -L$(GPHOME)/lib -lpq
CFLAGS = $(PCFLAGS) -I$(BOOST_ROOT) 
LDFLAGS = $(PLDFLAGS) -lgp -L$(BOOST_ROOT)/stage/lib -lboost_system -lboost_thread \
	-lboost_program_options -lboost_filesystem -lboost_chrono
OBJS = src/sql.o src/sqllex.o src/sql_base.o src/sql_tree.o src/sql_statement.o \
	src/sql_select.o src/sql_insert.o src/sql_update.o src/sql_delete.o \
	src/sql_extern.o src/struct_dynamic.o src/sql_control.o src/sql_create.o \
	src/sql_drop.o src/sql_truncate.o
BUILDOBJ = src/buildsql.o

ifeq "$(HAS_YACC)" ""
SKIPS = src/sql.cpp src/sql.cpp.h src/sqllex.cpp
else
TMPS = src/sql.cpp src/sql.cpp.h src/sqllex.cpp
endif

ORACLE_OBJS = src/oracle_control.o
ALTIBASE_OBJS = src/altibase_control.o
GP_OBJS = src/gp_control.o src/gp_strconv.o

BINS = buildsql
LIBS = libsqlcommon.$(SO)
ifdef ORACLE_HOME
LIBS := $(LIBS) libsqloracle.$(SO)
endif
ifdef ALTIBASE_HOME
LIBS := $(LIBS) libsqlaltibase.$(SO)
endif
ifdef GPHOME
LIBS := $(LIBS) libsqlgp.$(SO)
endif
TARGET = $(LIBS) $(BINS)

all:$(TARGET) install

buildsql:$(BUILDOBJ) libsqlcommon.$(SO)
	$(CC) $(CFLAGS) -o $@ $(BUILDOBJ) -L. -lsqlcommon $(LDFLAGS)

libsqlcommon.$(SO):$(OBJS)
	$(CC) $(CFLAGS) $(SOFLAGS) -o $@ $(OBJS) $(LDFLAGS)

libsqloracle.$(SO):$(ORACLE_OBJS) libsqlcommon.$(SO)
	$(CC) $(CFLAGS) $(SOFLAGS) -o $@ $(ORACLE_OBJS) -L. -lsqlcommon $(LDFLAGS) $(ORACLE_LIB)

libsqlaltibase.$(SO):$(ALTIBASE_OBJS) libsqlcommon.$(SO)
	$(CC) $(CFLAGS) $(SOFLAGS) -o $@ $(ALTIBASE_OBJS) -L. -lsqlcommon $(LDFLAGS) $(ALTIBASE_LIB)
	
libsqlgp.$(SO):$(GP_OBJS) libsqlcommon.$(SO)
	$(CC) $(CFLAGS) $(SOFLAGS) -o $@ $(GP_OBJS) -L. -lsqlcommon $(LDFLAGS) $(GP_LIB)

ifneq "$(HAS_YACC)" ""

src/sql.cpp: src/sql.y
	yacc -d -o $@ $<

src/sqllex.cpp: src/sql.l
	lex -o$@ $<

endif

$(ORACLE_OBJS): %.o: %.cpp
	$(CC) -c $(CFLAGS) $(ORACLE_HEADERS) -o $@ $<

$(ALTIBASE_OBJS): %.o: %.cpp
	$(CC) -c $(CFLAGS) $(ALTIBASE_HEADERS) -o $@ $<

$(GP_OBJS): %.o: %.cpp
	$(CC) -c $(CFLAGS) $(GP_HEADERS) -o $@ $<

include $(SGROOT)/etc/env_suffix.mk
