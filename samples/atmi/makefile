include $(SGROOT)/etc/env_public.mk
CFLAGS = $(PCFLAGS) -I$(BOOST_ROOT)
LDFLAGS = $(PLDFLAGS) -lsg
BINS = server client
SAMPLES = atmi
TARGET = $(BINS)

SOBJS = src/svc_example.o
COBJS = src/clt_example.o

all:$(TARGET) install

server:$(SOBJS)
	sgcompile --server --output=$@ --first="$(CFLAGS) $(SOBJS)" --operation=SERVICE:svc_example

client:$(COBJS)
	sgcompile --client --output=$@ --first="$(CFLAGS) $(COBJS)" --last="$(LDFLAGS)"

include $(SGROOT)/etc/env_suffix.mk
