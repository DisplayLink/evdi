#
# Copyright (c) 2015 - 2016 DisplayLink (UK) Ltd.
#

DEPS = evdi_ioctl.h
CFLAGS := -I../module -std=gnu99 -fPIC $(CFLAGS)

default: libevdi.so

clean:
	rm -rf *.o libevdi.so

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

libevdi.so: evdi_lib.o
	$(CC) $^ -o $@ -lc -lgcc -shared -Wl,-soname,libevdi.so.0

install: libevdi.so evdi_lib.h
	mkdir -p $(PREFIX)/include $(PREFIX)/lib
	install -m 0644 -D evdi_lib.h $(PREFIX)/include/evdi_lib.h
	install -m 0644 -D libevdi.so $(PREFIX)/lib/libevdi.so.0.$(VERSION)
	ln -s libevdi.so.0.$(VERSION) $(PREFIX)/lib/libevdi.so.0
	ln -s libevdi.so.0.$(VERSION) $(PREFIX)/lib/libevdi.so
