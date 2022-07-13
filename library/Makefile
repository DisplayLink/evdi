#
# Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
#

INSTALL ?= install
CP ?= cp
LN ?= ln
RM ?= rm

DEPS = evdi_ioctl.h
CFLAGS := -I../module -std=gnu99 -fPIC -D_FILE_OFFSET_BITS=64 $(CFLAGS)

LIBVER := 1.12.0
LIBABI := 0

PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib

default: libevdi.so.$(LIBVER)

clean:
	rm -rf *.o libevdi.so.$(LIBVER) libevdi.so

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ -c $<

libevdi.so.$(LIBVER): evdi_lib.o
	$(CC) $^ -shared -Wl,-soname,libevdi.so.$(LIBABI) -o $@ -lc -lgcc $(LDFLAGS)
	$(CP) libevdi.so.$(LIBVER) libevdi.so

install:
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 755 libevdi.so.$(LIBVER) $(DESTDIR)$(LIBDIR)/libevdi.so.$(LIBVER)
	$(LN) -sf libevdi.so.$(LIBVER) $(DESTDIR)$(LIBDIR)/libevdi.so.$(LIBABI)
	$(LN) -sf libevdi.so.$(LIBABI) $(DESTDIR)$(LIBDIR)/libevdi.so

uninstall:
	$(RM) -f $(DESTDIR)$(LIBDIR)/libevdi.so.$(LIBVER)
	$(RM) -f $(DESTDIR)$(LIBDIR)/libevdi.so.$(LIBABI)
	$(RM) -f $(DESTDIR)$(LIBDIR)/libevdi.so
