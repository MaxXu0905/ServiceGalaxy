include $(SGROOT)/etc/env_public.mk
CFLAGS = $(PCFLAGS)
LDFLAGS = $(PLDFLAGS) -lsg
BINS = sgpack sgunpack sgcompile sgboot sgshut sgclst sgmngr sghgws sggws sgadmin sgagent \
	sgrecover sgclear metarep fmngr kmngr sgpasswd
TARGET = $(BINS)

MNGR_SHARE = src/bbm_svc.o src/bbm_rpc.o src/bbm_chk.o src/bbm_ctx.o src/bb_switch.o
PACK_OBJS = src/sg_loadcf.o
UNPACK_OBJS = src/sg_unloadcf.o
COMPILE_OBJS = src/sg_build.o src/build_main.o
BOOT_OBJS = src/sg_boot.o src/boot_main.o src/boot_switch.o src/bs_base.o
SHUT_OBJS = src/sg_shut.o src/shut_main.o src/boot_switch.o src/bs_base.o
CLST_OBJS = src/dbbm_server.o src/dbbm_switch.o src/dbbm_rte.o $(MNGR_SHARE)
MNGR_OBJS = src/bbm_server.o src/bbm_switch.o src/bbm_rte.o $(MNGR_SHARE)
GWS_OBJS = src/gwp_ctx.o src/gws_rpc.o src/gws_server.o src/gws_queue.o src/gws_connection.o \
	src/gws_svc.o src/gws_switch.o
HGWS_OBJS = src/gwp_ctx.o src/gws_rpc.o src/gws_server.o src/gws_queue.o src/gws_connection.o \
	src/gws_svc.o src/bsgws_switch.o
ADMIN_OBJS = src/admin_server.o src/admin_main.o src/cmd_parser.o src/common_parser.o \
	src/bb_parser.o src/br_parser.o src/chg_parser.o src/prt_parser.o src/admin_switch.o
AGENT_OBJS = src/listener.o src/listener_connection.o src/listener_main.o
CLEAR_OBJS = src/cleanupsvr.o
RECOVER_OBJS = src/restartsvr.o
METAREP_OBJS = src/mrp_ctx.o src/metarep_svr.o src/metarep_svc.o
FMNGR_OBJS = src/file_svr.o src/file_svc.o
KMNGR_OBJS = src/alive_svr.o src/alive_svc.o
PASSWD_OBJS = src/passwd_main.o

all:$(COMPILE_OBJS)
	@$(MAKE) sgcompile
	@$(MAKE) ALL

ALL:$(TARGET) install

sgpack:$(PACK_OBJS)
	$(CC) $(CFLAGS) -o $@ $(PACK_OBJS) $(LDFLAGS)

sgunpack:$(UNPACK_OBJS)
	$(CC) $(CFLAGS) -o $@ $(UNPACK_OBJS) $(LDFLAGS)

sgcompile:$(COMPILE_OBJS)
	$(CC) $(CFLAGS) -o $@ $(COMPILE_OBJS) $(LDFLAGS)

sgboot:$(BOOT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(BOOT_OBJS) $(LDFLAGS)

sgshut:$(SHUT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(SHUT_OBJS) $(LDFLAGS)

sgclst:$(CLST_OBJS) sgcompile
	sgcompile --server --output=$@ --first="$(CFLAGS) $(CLST_OBJS)" --server-class="dbbm_server" --operation="..MASTERBB:bbm_svc" --system

sgmngr:$(MNGR_OBJS) sgcompile
	sgcompile --server --output=$@ --first="$(CFLAGS) $(MNGR_OBJS)" --server-class="bbm_server" --operation="..ADJUNCTBB:bbm_svc" --system

sghgws:$(HGWS_OBJS) sgcompile
	sgcompile --server --output=$@ --first="$(CFLAGS) $(HGWS_OBJS)" --server-class="gws_server" --operation="..GWS:gws_svc" --system

sggws:$(GWS_OBJS) sgcompile
	sgcompile --server --output=$@ --first="$(CFLAGS) $(GWS_OBJS)" --server-class="gws_server" --operation="..GWS:gws_svc" --system

sgadmin:$(ADMIN_OBJS)
	$(CC) $(CFLAGS) -o $@ $(ADMIN_OBJS) $(LDFLAGS)

sgagent:$(AGENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(AGENT_OBJS) $(LDFLAGS)

sgclear:$(CLEAR_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CLEAR_OBJS) $(LDFLAGS)

sgrecover:$(RECOVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $(RECOVER_OBJS) $(LDFLAGS)

metarep:$(METAREP_OBJS) sgcompile
	sgcompile --server --output=$@ --first="$(CFLAGS) $(METAREP_OBJS)" --server-class=metarep_svr --operation=METAREP:metarep_svc

fmngr:$(FMNGR_OBJS) sgcompile
	sgcompile --server --output=$@ --first="$(CFLAGS) $(FMNGR_OBJS)" --server-class=file_svr --operation=FMNGR_SVC:file_svc

kmngr:$(KMNGR_OBJS) sgcompile
	sgcompile --server --output=$@ --first="$(CFLAGS) $(KMNGR_OBJS)" --server-class=alive_svr --operation=ALIVE_SVC:alive_svc

sgpasswd:$(PASSWD_OBJS)
	$(CC) $(CFLAGS) -o $@ $(PASSWD_OBJS) $(LDFLAGS)

include $(SGROOT)/etc/env_suffix.mk
