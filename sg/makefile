include $(SGROOT)/etc/env_public.mk
CFLAGS = $(PCFLAGS) -I$(ZLIB_ROOT)/include
LDFLAGS = $(PLDFLAGS) -lgp -L$(BOOST_ROOT)/stage/lib -lboost_system -lboost_thread \
	-lboost_program_options -lboost_filesystem -lboost_chrono -L$(ZLIB_ROOT)/lib -lz
LIBS = libsg.$(SO)
TARGET = $(LIBS)

OBJS = src/sgp_ctx.o src/sgt_ctx.o src/sgc_ctx.o src/sg_exception.o src/sg_message.o \
	src/sg_struct.o src/sg_manager.o src/sg_xmlcfg.o src/sg_config.o src/sg_pte.o src/sg_nte.o \
	src/sg_rte.o src/sg_sgte.o src/sg_ste.o src/sg_scte.o src/sg_qte.o src/sg_proc.o src/sg_api.o \
	src/sg_lock.o src/sg_ipc.o src/sg_options.o src/sg_viable.o src/sg_agent.o src/sg_rpc.o \
	src/sg_switch.o src/sg_svcdsp.o src/sg_svr.o src/sg_handle.o src/sg_meta.o src/sg_svc.o \
	src/file_struct.o src/file_rpc.o src/alive_rpc.o

all:$(TARGET) install

libsg.$(SO):$(OBJS)
	$(CC) $(CFLAGS) $(SOFLAGS) -o $@ $(OBJS) $(LDFLAGS)

include $(SGROOT)/etc/env_suffix.mk
