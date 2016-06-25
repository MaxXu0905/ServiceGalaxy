ARGS=all
ALL=framework helpers sg dbc tests

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
