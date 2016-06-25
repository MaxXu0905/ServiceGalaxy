TEST_LIBDIR=../lib
TEST_HEADERDIR=../include
include $(SGROOT)/etc/env_public.mk
CFLAGS = $(PCFLAGS) $(ORACLE_HEADERS) -I$(SQLITE_ROOT)/include -I./include -I$(TEST_HEADERDIR)
LDFLAGS = $(PLDFLAGS) $(ORACLE_LIB) -L$(TEST_LIBDIR) -lgp -lsg -lsqldbc  -lpthread -ltestclient -lgtest \
		-ltesthelper -lsqloracle -lsqloracle -ldbcclient -ltestDbc
BINS = SGPTest DBCTest SDCTest SGRobustTest
TARGET = $(BINS)
all:$(TARGET) setshell
SGPTest : src/SGPTest.o 
	$(CC) $(CFLAGS) $(LDFLAGS) $(COBJS) src/SGPTest.o  -o$@
SGRobustTest :src/SGRobustTest.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(COBJS) src/SGRobustTest.o  -o$@
DBCTest :src/DBCTest.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(COBJS) src/DBCTest.o  -o$@
	
SDCTest :src/SDCTest.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(COBJS) src/SDCTest.o  -o$@
.SUFFIXES:.o .h
.SUFFIXES:.cpp .o
.cpp.o:
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) src/*.o

setshell:
	chmod 755 shell/*

remake: 
	$(MAKE) clean all
