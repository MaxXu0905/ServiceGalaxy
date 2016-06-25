ARGS=all

ALL=public
ifdef ORACLE_HOME
ifndef HPACC_WITH_AP
ALL := $(ALL) pubocci
endif
endif
ALL := $(ALL) gp sql pubapp sg cmd monitor dbc samples

.NOTPARALLEL: $(ALL)

all: $(ALL)
  @:

$(ALL)::
	cd $@; $(MAKE) $(ARGS)

install clean:
	@$(MAKE) ARGS=$@

remake:
	@$(MAKE) ARGS=clean
	@$(MAKE) ARGS=all
