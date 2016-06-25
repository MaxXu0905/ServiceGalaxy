ARGS=all

ALL=atmi sql pubapp dbc
all: $(ALL)
  @:

$(ALL)::
	cd $@; $(MAKE) $(ARGS)

install clean:
	@$(MAKE) ARGS=$@

remake:
	@$(MAKE) ARGS=clean
	@$(MAKE) ARGS=all
