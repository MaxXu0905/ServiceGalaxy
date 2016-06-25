include $(SGROOT)/etc/env_public.mk
CFLAGS = $(PCFLAGS)
LDFLAGS = $(PLDFLAGS) -lgp
OBJS = src/shm_compiler.o src/shm_malloc.o src/shm_pair.o
BINS = shmadmin
LIBS = libpubapp.$(SO)

TARGET = $(LIBS) $(BINS)

SHMADMINSRC = src/shm_admin.o

all:$(TARGET) install

libpubapp.$(SO):$(OBJS)
	$(CC) $(CFLAGS) $(SOFLAGS) -o $@ $(OBJS) $(LDFLAGS)

shmadmin: $(SHMADMINSRC) libpubapp.$(SO)
	$(CC) $(CFLAGS) -o $@ $(SHMADMINSRC) -L. -lpubapp $(LDFLAGS)

include $(SGROOT)/etc/env_suffix.mk
