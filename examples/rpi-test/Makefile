LIBSRC = ../../
-include ../../Makefile

all: vmmio-test-inside vmmio-test-outside

vmmio-test-inside: vmmio-test-inside.c
	$(AARCH64CC) $(CFLAGS) -o$@ $^
vmmio-test-outside: vmmio-test-outside.c libcoreio.a
	$(HOSTCC) $(CFLAGS) -o$@ $^

clean:
	rm -f libcoreio.so libcoreio.a vmmio-test-inside vmmio-test-outside
