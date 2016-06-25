TEST_LIBDIR=../lib
TEST_HEADERDIR=../include
include $(SGROOT)/etc/env_public.mk
CFLAGS = $(PCFLAGS) $(ORACLE_HEADERS) -I$(SQLITE_ROOT)/include -I./include -I$(TEST_HEADERDIR)
LDFLAGS = $(PLDFLAGS) $(ORACLE_LIB) -lsqloracle -lsqloracle -ldbcclient

SOOBJS = src/DBTemplate.o src/DBCTestContext.o  src/dbc_test_select.o src/dbc_test_insert.o src/dbc_test_update.o src/dbc_test_delete.o \
	src/dbc_test_mselect.o src/dbc_test_mupdate.o src/dbc_test_mdelete.o src/test_ctx.o src/test_manager.o src/test_thread.o

BINS = 
LIBS=libtestDbc.so
TARGET = $(BINS) $(LIBS)

all:
	$(MAKE) code_gen
	$(MAKE) ALL

ALL: $(TARGET) install

libtestDbc.so:$(SOOBJS)
	$(CC) $(CFLAGS) $(SOFLAGS) -o $@ $(SOOBJS) $(LDFLAGS)
ifndef DEBUG
	-$(STRIP) $@
endif

.SUFFIXES:.o .h
.SUFFIXES:.cpp .o
.cpp.o:
	$(CC) -c $(CFLAGS) -o $@ $<

code_gen: buildstatic.sh
	touch code_gen
	sh buildstatic.sh

clean:
	rm -f src/*.o $(TARGET)

install: $(TARGET)
	-mkdir -p $(APPDIR)/lib
	cp $(LIBS) $(APPDIR)/lib/

	-mkdir -p ${TEST_LIBDIR}/
	cp $(LIBS) ${TEST_LIBDIR}/
	-mkdir -p  $(TEST_HEADERDIR)/
	cp -r include/* $(TEST_HEADERDIR)/

remake: 
	$(MAKE) clean all
