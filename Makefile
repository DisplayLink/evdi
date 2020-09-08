#
# Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
#

FLAGS=-Werror -Wextra -Wall -Wmissing-prototypes -Wstrict-prototypes -Wno-error=missing-field-initializers

EL8 := $(shell cat /etc/redhat-release | grep -c " 8." )
ifneq (,$(findstring 1, $(EL8)))
FLAGS:=$(FLAGS) -D EL8
endif

all:
	CFLAGS="$(FLAGS)" $(MAKE) -C module $(MFLAGS)
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

