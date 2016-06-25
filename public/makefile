include env_public.mk
CFLAGS = $(PCFLAGS)
LDFLAGS = -L$(SGROOT)/lib $(PUBFLAGS) -L$(BOOST_ROOT)/stage/lib -lboost_locale \
	-lboost_thread -L$(SSH2_ROOT)/lib -lssh2
LIBS = libpublic.$(SO)
TMPS = sg.mo
ETCS = env_public.mk env_suffix.mk
TARGET = $(LIBS) sg.mo

OBJS = src/sg_locale.o src/common.o src/log_file.o src/user_exception.o src/ftp.o \
	src/dispatchtask.o src/ipsocket.o src/memleak.o src/msgqueue.o src/semmutex.o \
	src/udpsocket.o src/crontab.o src/tcpsocket.o src/calculator.o src/simple_reg.o \
	src/uhash.o src/perf_stat.o src/dstream.o src/machine.o src/asn1.o src/compiler.o \
	src/sshp_ctx.o

all:$(TARGET) install

libpublic.$(SO):$(OBJS)
	$(CC) $(CFLAGS) $(SOFLAGS) -o $@ $(OBJS) $(LDFLAGS)

sg.mo: sg.po
	msgfmt -o $@ $<
	mkdir -p $(SGROOT)/etc/zh_CN/LC_MESSAGES
	cp $@ $(SGROOT)/etc/zh_CN/LC_MESSAGES/

text:
	find .. -name *.cpp -o -name *.h > all_files.txt
	xgettext --keyword=_ --from-code=GBK --no-location -f all_files.txt -x sg.po -o sg_new.po
	rm -f all_files.txt

include env_suffix.mk
