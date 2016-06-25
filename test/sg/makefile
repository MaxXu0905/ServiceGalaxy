include $(SGROOT)/etc/env_public.mk

TEST_LIBDIR=../lib
TEST_HEADERDIR=../include


CFLAGS = $(PCFLAGS) -I$(BOOST_ROOT) -I$(TEST_HEADERDIR) -I. -I./src
LDFLAGS = $(PLDFLAGS) -lsg -L$(TEST_LIBDIR) -ltesthelper
BINS = TestServer TestServerSendBack  TestServerAllInOne 1bootSequence 2bootSequence\
 3bootSequence TestMSSQ TestServerClopt TestServerUlog TestServerUlog2 TestAbortServer1 TestAbortServer2 
LIBS = libtestclient.so
TARGET = $(BINS) $(LIBS)

SOBJS = src/svc_test.o src/base_server_test.o
SSENDBACKOBJS = src/svc_test_sendback.o src/base_server_test.o
ALLINONEOBJS=src/svc_test.o src/svc_test_sendback.o src/svc_test_advertise.o  src/server_test.o
COBJS = src/clt_test.o
SEQUENCEOBJS=$(SOBJS) src/server_sequence.o
CLOPTOBJS=$(SOBJS) src/server_clopt.o
ULOGOBJS=$(SOBJS) src/server_ulog.o
ABORTOBJS=src/svc_test.o src/svc_test_coredump.o
all:$(TARGET) install

TestServer:$(SOBJS)
	sgcompile --server --output=$@ --first="$(CFLAGS) $(SOBJS)  $(LDFLAGS)" --server-class="base_server_test" --operation=TESTSERVICE:svc_test
ifndef DEBUG
	-$(STRIP) $@
endif
TestMSSQ:$(SOBJS)
	sgcompile --server --output=$@ --first="$(CFLAGS) $(SOBJS)  $(LDFLAGS)" --server-class="base_server_test" --operation=TESTMSSQ:svc_test
ifndef DEBUG
	-$(STRIP) $@
endif
TestServerSendBack:$(SSENDBACKOBJS)
	sgcompile --server --output=$@ --first="$(CFLAGS) $(SSENDBACKOBJS)  $(LDFLAGS)"  --server-class="base_server_test" --operation=TESTSERVICECALLBACK:svc_test_sendback
ifndef DEBUG
	-$(STRIP) $@
endif

TestServerAllInOne:$(ALLINONEOBJS)
	sgcompile --server --output=$@ --first="$(CFLAGS) $(ALLINONEOBJS)  $(LDFLAGS)" --server-class="server_test" --operation="TESTSERVICE2:svc_test" --operation="TESTSERVICECALLBACK2:svc_test_sendback" --operation="TESTSERVICEADVERTISE:svc_test_advertise" 
ifndef DEBUG
	-$(STRIP) $@
endif

1bootSequence:$(SEQUENCEOBJS)
	sgcompile --server --output=$@ --first="$(CFLAGS) $(SEQUENCEOBJS)  $(LDFLAGS)" --server-class="server_sequence" --operation="TESTSERVICESEQUENCE1:svc_test"  
ifndef DEBUG
	-$(STRIP) $@
endif
2bootSequence:$(SEQUENCEOBJS)
	sgcompile --server --output=$@ --first="$(CFLAGS) $(SEQUENCEOBJS)  $(LDFLAGS)" --server-class="server_sequence" --operation="TESTSERVICESEQUENCE2:svc_test"  
ifndef DEBUG
	-$(STRIP) $@
endif
3bootSequence:$(SEQUENCEOBJS)
	sgcompile --server --output=$@ --first="$(CFLAGS) $(SEQUENCEOBJS)  $(LDFLAGS)" --server-class="server_sequence" --operation="TESTSERVICESEQUENCE3:svc_test"  
ifndef DEBUG
	-$(STRIP) $@
endif
TestServerClopt:$(CLOPTOBJS)
	sgcompile --server --output=$@ --first="$(CFLAGS) $(CLOPTOBJS)  $(LDFLAGS)" --server-class="server_clopt" --operation="TESTCLOPTSERVICE:svc_test"  
ifndef DEBUG
	-$(STRIP) $@
endif
TestServerUlog:$(ULOGOBJS)
	sgcompile --server --output=$@ --first="$(CFLAGS) $(ULOGOBJS)  $(LDFLAGS)" --server-class="server_ulog" --operation="TESTULOGSERVICE:svc_test"  
ifndef DEBUG
	-$(STRIP) $@
endif
TestServerUlog2:$(ULOGOBJS)
	sgcompile --server --output=$@ --first="$(CFLAGS) $(ULOGOBJS)  $(LDFLAGS)" --server-class="server_ulog" --operation="TESTULOGSERVICE2:svc_test"  
ifndef DEBUG
	-$(STRIP) $@
endif

TestAbortServer1:$(ABORTOBJS)
	sgcompile --server --output=$@ --first="$(CFLAGS) $(ABORTOBJS)  $(LDFLAGS)"  --operation="TESTECHOSERVICE1:svc_test"  --operation="TESTABORTSERVICE1:svc_test_coredump"  
ifndef DEBUG
	-$(STRIP) $@
endif
TestAbortServer2:$(ABORTOBJS)
	sgcompile --server --output=$@ --first="$(CFLAGS) $(ABORTOBJS)  $(LDFLAGS)"  --operation="TESTECHOSERVICE2:svc_test"  --operation="TESTABORTSERVICE2:svc_test_coredump"  
ifndef DEBUG
	-$(STRIP) $@
endif

libtestclient.so:$(COBJS)
	$(CC) $(CFLAGS) $(SOFLAGS) -o $@ $(COBJS) $(LDFLAGS)
ifndef DEBUG
	-$(STRIP) $@
endif

.SUFFIXES:.o .h
.SUFFIXES:.cpp .o
.cpp.o:
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) src/*.o

install: $(TARGET)
ifdef BINS
	for i in $(BINS); do rm -f $(SGROOT)/bin/$$i; done
endif
	-mkdir -p $(APPDIR)/bin
	cp $(BINS) $(APPDIR)/bin/
	-mkdir -p $(APPDIR)/lib
	cp $(LIBS) $(APPDIR)/lib/
	
	-mkdir -p ${TEST_LIBDIR}/
	cp $(LIBS) ${TEST_LIBDIR}/
	-mkdir -p  $(TEST_HEADERDIR)/
	cp -r include/* $(TEST_HEADERDIR)/

remake: 
	$(MAKE) clean all
