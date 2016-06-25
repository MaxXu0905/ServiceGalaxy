include $(SGROOT)/etc/env_public.mk
CFLAGS = $(PCFLAGS) $(ORACLE_INCLUDES)
LDFLAGS = $(PLDFLAGS) -lgp -lsg -lpubapp
BINS = shm_vector shm_map shm_set
SAMPLES = pubapp
TARGET = $(BINS)

all:$(TARGET) install

shm_vector: src/vector_main.o
	$(CC) $(CFLAGS) -o $@ src/vector_main.o $(LDFLAGS)

shm_map: src/map_main.o
	$(CC) $(CFLAGS) -o $@ src/map_main.o $(LDFLAGS)

shm_set: src/set_main.o
	$(CC) $(CFLAGS) -o $@ src/set_main.o $(LDFLAGS)

include $(SGROOT)/etc/env_suffix.mk
