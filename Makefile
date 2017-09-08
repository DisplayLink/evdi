#
# Copyright (c) 2015 DisplayLink (UK) Ltd.
#

FLAGS=-Werror -Wextra -Wall -Wmissing-prototypes -Wstrict-prototypes
PREFIX?=/usr/local
VERSION=1.4.1

all: module library

module:
	CFLAGS="$(FLAGS)" $(MAKE) -C module $(MFLAGS)

library:
	CFLAGS="-I../module $(FLAGS) $(CFLAGS)" $(MAKE) -C library $(MFLAGS)

clean:
	$(MAKE) clean -C module $(MFLAGS)
	$(MAKE) clean -C library $(MFLAGS)

install:
	VERSION=$(VERSION) $(MAKE) -C library install
