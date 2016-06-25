include $(SGROOT)/etc/env_public.mk
CFLAGS = $(PCFLAGS) $(ORACLE_HEADERS) $(ORACLE_FLAGS)
LDFLAGS = $(PLDFLAGS) $(ORACLE_LIB) $(OCCI_LIB)
OBJS = src/array.o src/db_common.o src/hashtable.o src/semrwlock.o \
	src/sharememserver.o src/workthread.o src/common_hash.o src/database.o \
	src/db_exception.o src/object.o src/sharememclient.o
LIBS = libpubocci.$(SO)

TARGET = $(LIBS)

all:$(TARGET) install

libpubocci.$(SO):$(OBJS)
	$(CC) $(CFLAGS) $(SOFLAGS) -o $@ $(OBJS) $(LDFLAGS)

include $(SGROOT)/etc/env_suffix.mk
