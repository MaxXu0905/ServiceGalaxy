.SUFFIXES:.o .h
.SUFFIXES:.cpp .o
.SUFFIXES:.c .o
.SUFFIXES:.s .o

.cpp.o:
	$(CC) -c $(CFLAGS) -o $@ $<

.c.o:
	cc -c $(MFLAGS) -o $@ $<

.s.o:
	cc -c $(MFLAGS) -o $@ $<

clean:
ifdef SKIPS
	-for i in $(SKIPS); do mv $$i $$i.bak; done
endif
	-rm -rf src/*.o $(BINS) $(LIBS) $(TMPS)
ifdef SKIPS
	-for i in $(SKIPS); do mv $$i.bak $$i; done
endif

install: $(TARGET)
ifndef SAMPLES

ifdef BINS
	-mkdir -p $(SGROOT)/bin
	-for i in $(BINS); do rm -f $(SGROOT)/bin/$$i; done
	cp $(BINS) $(SGROOT)/bin/
	-chmod +x $(SGROOT)/bin/*.sh
ifndef DEBUG
	-for i in $(BINS); do $(STRIP) $(SGROOT)/bin/$$i; done
endif
endif

ifdef LIBS
	-mkdir -p $(SGROOT)/lib
	-for i in $(LIBS); do rm -f $(SGROOT)/lib/$$i; done
	cp $(LIBS) $(SGROOT)/lib/
ifndef DEBUG
	-for i in $(LIBS); do $(STRIP) $(SGROOT)/lib/$$i; done
endif
endif

ifdef INCLUDES
	-mkdir -p $(SGROOT)/include
	-for i in $(INCLUDES); do rm -f $(SGROOT)/include/$$i; done
	cp $(INCLUDES) $(SGROOT)/include/
endif

ifdef SRCS
	-mkdir -p $(SGROOT)/src
	-for i in $(SRCS); do rm -f $(SGROOT)/src/$$i; done
	cp $(SRCS) $(SGROOT)/src/
endif

ifdef ETCS
	-mkdir -p $(SGROOT)/etc
	-for i in $(ETCS); do rm -f $(SGROOT)/etc/$$i; done
	cp $(ETCS) $(SGROOT)/etc/
endif

else

	-rm -rf $(SGROOT)/samples/$(SAMPLES)
	-mkdir -p $(SGROOT)/samples/$(SAMPLES)
	-cp -R . $(SGROOT)/samples/$(SAMPLES)/
	-rm -rf $(SGROOT)/samples/$(SAMPLES)/.svn $(SGROOT)/samples/$(SAMPLES)/*/.svn

endif

remake: 
	make clean all
