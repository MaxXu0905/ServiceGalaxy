include $(SGROOT)/etc/env_public.mk
CFLAGS = -DHAVE_CONFIG_H  $(PCFLAGS) -I. -I./include -I./gen-cpp \
	-I$(THRIFT_ROOT)/include/thrift -I$(LIBEVENT_ROOT)/include \
	-I$(OPENSSL_ROOT)/include
LDFLAGS = $(PLDFLAGS) -L$(THRIFT_ROOT)/lib -lthrift  -lthriftz -lthriftnb \
	-L$(BOOST_ROOT)/stage/lib -lboost_program_options -lboost_system\
	-L$(LIBEVENT_ROOT)/lib -L$(OPENSSL_ROOT)/lib -levent -lcrypto -lgp -lsg
PCH_H = include/monitorpch.h
PCH = include/monitorpch.h.gch
BINS = sgtgws
INCLUDES = 
TMPS = src/*.o gen-cpp $(PCH) gen

TARGET = $(BINS)
THRIFTOBJS=gen-cpp/Monitor.o gen-cpp/monitor_types.o gen-cpp/monitor_constants.o 
BASEOBJS = src/SGMonitor.o src/ServerManager.o src/SGAdaptor.o src/MonitorHandler.o \
	src/MonitorLog.o src/Config.o src/MonitorSetter.o src/TaskManager.o \
	$(THRIFTOBJS)
MAINOBJS = src/MonitorMain.o $(BASEOBJS)
SGSERVER = src/monitor_server.o $(BASEOBJS)
CLIENTOBJ=src/TGWSClient.o $(THRIFTOBJS)

all:
	@$(MAKE) gen
	@$(MAKE) ALL
	@$(MAKE) install
	
ALL:$(TARGET) 

MonitorMain:$(MAINOBJS)
	$(CC) $(CFLAGS) -o $@ $(MAINOBJS) $(LDFLAGS)

TGWSclient:$(CLIENTOBJ)
	$(CC) $(CFLAGS) -o $@ $(CLIENTOBJ) $(LDFLAGS)

sgtgws:$(SGSERVER)
	sgcompile --server --output=$@ --first="$(CFLAGS) $(SGSERVER) $(LDFLAGS)" --server-class="monitor_server" --operation=".TGWS:monitor_svc" --system
	
gen:$(wildcard *.thrift)
	touch gen
	thrift --gen cpp:templates,cob_style -r $<

include $(SGROOT)/etc/env_suffix.mk
