#
# Copyright (c) 2015 DisplayLink (UK) Ltd.
#

all:
	$(MAKE) -C module $(MFLAGS)
	$(MAKE) -C library $(MFLAGS)

clean:
	$(MAKE) clean -C module $(MFLAGS)
	$(MAKE) clean -C library $(MFLAGS)

