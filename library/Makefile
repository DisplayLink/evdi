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

