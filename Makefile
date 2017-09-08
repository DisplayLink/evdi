#
# Copyright (c) 2015 DisplayLink (UK) Ltd.
#

FLAGS=-Werror -Wextra -Wall -Wmissing-prototypes -Wstrict-prototypes

all: module library

module:
	CFLAGS="$(FLAGS)" $(MAKE) -C module $(MFLAGS)

library:
	CFLAGS="-I../module $(FLAGS) $(CFLAGS)" $(MAKE) -C library $(MFLAGS)

clean:
	$(MAKE) clean -C module $(MFLAGS)
	$(MAKE) clean -C library $(MFLAGS)

