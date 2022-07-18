#
# Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
#

FLAGS=-Werror -Wextra -Wall -Wno-error=missing-field-initializers -Werror=sign-compare
FLAGS_C=$(FLAGS) -Wmissing-prototypes -Wstrict-prototypes -Werror=discarded-qualifiers
FLAGS_CXX=$(FLAGS)

all:
	CFLAGS="-isystem./include -isystem./include/uapi $(FLAGS_C) $(CFLAGS)" $(MAKE) -C module $(MFLAGS)
	CFLAGS="-I../module $(FLAGS_C) $(CFLAGS)" $(MAKE) -C library $(MFLAGS)
	CXXFLAGS="-I../module -I../library $(FLAGS_CXX) $(CXXFLAGS)" $(MAKE) -C pyevdi $(MFLAGS)

install:
	$(MAKE) -C module install
	$(MAKE) -C library install
	$(MAKE) -C pyevdi install

uninstall:
	$(MAKE) -C module uninstall
	$(MAKE) -C library uninstall
	$(MAKE) -C pyevdi uninstall

clean:
	$(MAKE) clean -C module $(MFLAGS)
	$(MAKE) clean -C library $(MFLAGS)
	$(MAKE) clean -C pyevdi $(MFLAGS)
