include $(SGROOT)/etc/env_public.mk
CFLAGS = $(PCFLAGS)
LDFLAGS = $(PLDFLAGS) -L$(BOOST_ROOT)/stage/lib -lboost_system -lboost_thread \
	-lboost_program_options -lboost_filesystem -lboost_chrono
LIBS = libgp.$(SO)
TARGET = $(LIBS)

OBJS = src/gpp_ctx.o src/gpt_ctx.o src/gpenv.o src/sys_func.o \
	src/ushm.o src/usignal.o src/uunix.o

ifeq "$(OS)" "HP-UX"
OBJS := $(OBJS) src/hpux_ts.o src/hpux_semclear.o
endif

all:$(TARGET) install

libgp.$(SO):$(OBJS)
	$(CC) $(CFLAGS) $(SOFLAGS) -o $@ $(OBJS) $(LDFLAGS)

include $(SGROOT)/etc/env_suffix.mk
