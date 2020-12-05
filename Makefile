#
# Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
#

FLAGS=-Werror -Wextra -Wall -Wmissing-prototypes -Wstrict-prototypes -Wno-error=missing-field-initializers

all:
	CFLAGS="$(FLAGS) $(CFLAGS)" $(MAKE) -C module $(MFLAGS)
	CFLAGS="-I../module $(FLAGS) $(CFLAGS)" $(MAKE) -C library $(MFLAGS)

install:
	$(MAKE) -C module install
	$(MAKE) -C library install

uninstall:
	$(MAKE) -C module uninstall
	$(MAKE) -C library uninstall

clean:
	$(MAKE) clean -C module $(MFLAGS)
	$(MAKE) clean -C library $(MFLAGS)
