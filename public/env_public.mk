ifdef ORACLE_HOME

include $(ORACLE_HOME)/rdbms/lib/env_rdbms.mk

ORACLE_HEADERS = -I$(ORACLE_HOME)/precomp/public -I$(ORACLE_HOME)/rdbms/demo -I$(ORACLE_HOME)/rdbms/public

ifndef ORACLE_LIBPATH
	ORACLE_LIBPATH=$(ORACLE_HOME)/lib
endif

ifdef ORA9
	ORACLE_LIB = -L$(ORACLE_LIBPATH) -lclntsh
	OCCI_LIB = -locci9
	ORACLE_FLAGS = -DORACLE_V9204
else
	ORACLE_LIB = -L$(ORACLE_LIBPATH) -lclntsh
	OCCI_LIB = -locci
	ORACLE_FLAGS = 
endif

endif

HEADERS = -I. -Iinclude -I$(SGROOT)/include -I$(BOOST_ROOT) -I$(SSH2_ROOT)/include

ifdef MEM_DEBUG
	MDFLAGS = -DMEM_DEBUG
endif

ifeq "$(OS)" "HP-UX"

ifeq "$(MODE)" "64bit"
	MFLAGS = -mt +DD64
else
	MFLAGS = -mt +Z
endif

ifdef DEBUG
	DFLAGS = -DDEBUG -DPERF_STAT -g +d
else
	DFLAGS = +O2 +inline_level2
endif

ifdef HPACC_WITH_AP
	AFLAGS = -AP -D__HPACC_USING_MULTIPLIES_IN_FUNCTIONAL -DORA_CPP
else
	AFLAGS = -AA
endif
	
	CC = aCC
	PCFLAGS = $(HEADERS) -DHPUX -w $(AFLAGS) $(DFLAGS) $(MFLAGS) $(MDFLAGS)
ifeq "$(shell uname -m)" "ia64"
	PUBFLAGS = -lpthread -lm -lnsl -lrt -lcl -lCsup -lstd_v2 -lunwind
else
ifeq "$(MODE)" "64bit"
	PUBFLAGS = -lpthread -lm -lc -lnsl -lrt -ldl -lcl -lCsup_v2 -lstd_v2
else
	PUBFLAGS = -lpthread -lm -lc -lnsl -lrt -lcl -lCsup_v2 -lstd_v2
endif
endif
	ARFLAG = rcu
	SOFLAGS = -b -dynamic
	STRIP = strip -l
	SO = sl
endif

ifeq "$(OS)" "AIX"

ifeq "$(MODE)" "64bit"
	MFLAGS = -q64
	MLFLAGS = -X64
endif

ifdef DEBUG
	DFLAGS = -DDEBUG -DPERF_STAT -g -qfullpath
else
	DFLAGS = -O2 -qstaticinline
endif
	
	CC = xlC_r -w -qmaxmem=-1 -qrtti=all
	PCFLAGS = -bmaxdata:0x80000000 -qthreaded $(HEADERS) -DAIX -bhalt:5 $(DFLAGS) $(MFLAGS) $(MDFLAGS)
	PUBFLAGS = -lpthread -lm -lc -lnsl -lrt -ldl -brtl
	ARFLAG = $(MLFLAGS) rcu
	SOFLAGS = -G -bM:SRE -bnoentry -qrtti=all -qmkshrobj
	STRIP = strip $(MLFLAGS)
	SO = so
endif

ifeq "$(OS)" "Linux"

ifeq "$(MODE)" "64bit"
	MFLAGS = -D_LARGEFILE64_SOURCE=1 -m64
else
	MFLAGS = -D_LARGEFILE_SOURCE=1 -m32
endif

ifdef DEBUG
	DFLAGS = -DDEBUG -DPERF_STAT -g
else
	DFLAGS = -O2 -finline-functions
endif
	
	CC = g++
	PCFLAGS = $(HEADERS) -DLINUX -fPIC -D_GNU_SOURCE -DSLTS_ENABLE -DSLMXMX_ENABLE -D_REENTRANT -DNS_THREADS -Wall -Wno-invalid-offsetof -Wno-sign-compare -fno-strict-aliasing $(DFLAGS) $(MFLAGS) $(MDFLAGS)
	PUBFLAGS = -lpthread -lm -lc -lnsl -lrt -ldl
	ARFLAG = rcu
	SOFLAGS = -shared
	STRIP = strip -g
	SO = so
endif

ifeq "$(OS)" "SunOS"

ifeq "$(MODE)" "64bit"
	MFLAGS = -xarch=v9 -w -mt
else
	MFLAGS = -mt
endif

ifdef DEBUG
	DFLAGS = -DDEBUG -DPERF_STAT -g
else
	DFLAGS = -O2
endif
	
	CC = CC
	PCFLAGS = $(HEADERS) -DSOLARIS -Kpic $(DFLAGS) $(MFLAGS) $(MDFLAGS)
	PUBFLAGS = -lpthread -lm -lc -lnsl -lrt -ldl -lsunmath -mt $(MFLAGS)
	ARFLAG = -cr 
	SOFLAGS = -G -Kpic
	STRIP = strip 
	SO = so
endif

PLDFLAGS = -L$(SGROOT)/lib $(PUBFLAGS) -lpublic
BINS =
LIBS =
SKIPS = 
INCLUDES = $(wildcard include/*.h) $(wildcard include/*.inl)
ETCS =
SAMPLES = 
TMPS =
