-include Makefile.local

AARCH64CC = $(CROSSPREFIX)gcc
HOSTCC = $(PREFIX)gcc
HOSTAR = $(PREFIX)ar
CFLAGS = -g -Wall -I$(LIBSRC)
LIBSRC ?=

all: libcoreio.so libcoreio.a

libcoreio.so: $(LIBSRC)corehdl-base.c $(LIBSRC)coreio.c $(LIBSRC)corehdl.h $(LIBSRC)coreio.h
	$(HOSTCC) $(CFLAGS) -shared -fPIC -o $@ $(LIBSRC)corehdl-base.c $(LIBSRC)coreio.c
	@chmod a-x $@
libcoreio.a: $(LIBSRC)corehdl-base.c $(LIBSRC)coreio.c $(LIBSRC)corehdl.h $(LIBSRC)coreio.h
	$(HOSTCC) $(CFLAGS) -c -o corehdl-base.o $(LIBSRC)corehdl-base.c
	$(HOSTCC) $(CFLAGS) -c -o coreio.o $(LIBSRC)coreio.c
	$(HOSTAR) cr $@ corehdl-base.o coreio.o
	@rm -f corehdl-base.o coreio.o

clean:
	rm -f libcoreio.so libcoreio.a
