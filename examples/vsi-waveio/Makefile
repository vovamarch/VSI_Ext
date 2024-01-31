LIBSRC = ../../
-include ../../Makefile

all: vsi-waveio

vsi-waveio: vsi-waveio.c libcoreio.a
	$(HOSTCC) $(CFLAGS) -o$@ $^

clean:
	rm -f libcoreio.so libcoreio.a vsi-waveio
