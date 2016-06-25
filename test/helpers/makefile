TEST_LIBDIR=../lib
TEST_HEADERDIR=../include
CFLAGS = -DHAVE_CONFIG_H -g -Wall -fPIC -Wextra   -I. -I./include -I./gen-cpp \
	-I$(BOOST_ROOT) -I$(THRIFT_ROOT)/include/thrift -I$(LIBEVENT_ROOT)/include
LDFLAGS = -L$(THRIFT_ROOT)/lib -lthrift  -lthriftz -lthriftnb \
	-L$(BOOST_ROOT)/stage/lib -lboost_program_options -lboost_system \
	-L$(LIBEVENT_ROOT)/lib -levent 
	
BINS = 
LIBS = libtesthelper.so libtesthelper.a
TARGET = $(BINS) $(LIBS)
OBJS=src/NetMessageClient.o src/NetMessageServer.o src/NetMessageConfig.o src/ServerManager.o src/FileUtils.o \
gen-cpp/Message.o gen-cpp/Message_types.o gen-cpp/Message_constants.o src/sys.o src/Config.o
all:
	$(MAKE) gen
	$(MAKE) ALL
	$(MAKE) install

ALL:$(TARGET) 

libtesthelper.so:$(OBJS)
	$(CC) -shared $(CFLAGS) $(LDFLAGS) $(OBJS) -o$@
	
libtesthelper.a:$(OBJS)
	ar -rc $@  $(OBJS)
	
.SUFFIXES:.o .h
.SUFFIXES:.cpp .o
.cpp.o:
	$(CC) -c $(CFLAGS) -o $@ $<

gen:$(wildcard *.thrift)
	touch gen
	thrift --gen cpp:templates,cob_style -r $<

clean:
	rm -f $(TARGET) $(OBJS)
	rm -rf  gen-cpp gen

install: $(TARGET)
	mkdir -p $(SGROOT)/lib
	cp libtesthelper.so $(SGROOT)/lib/
	
	mkdir -p ${TEST_LIBDIR}/
	cp $(LIBS) ${TEST_LIBDIR}/
	mkdir -p  $(TEST_HEADERDIR)/
	cp -r include/*.h $(TEST_HEADERDIR)/

remake: 
	$(MAKE) clean all
