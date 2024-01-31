LIBSRC = ../../
-include ../../Makefile

all: vsi-ext

vsi-ext: vsi-ext.c libcoreio.a
	$(HOSTCC) $(CFLAGS) -o$@ $^ -I/usr/include/python3.8 -L/usr/lib/python3.8/config-3.8-aarch64-linux-gnu -L/usr/lib  -lcrypt -lpthread -ldl -lpython3.8 -lutil -lm -lm -Wno-unused-result -Wsign-compare -g -fdebug-prefix-map=/build/python3.8-oFzxjd/python3.8-3.8.10=. -specs=/usr/share/dpkg/no-pie-compile.specs -fstack-protector -Wformat -Werror=format-security  -DNDEBUG -g -fwrapv -O3 -Wall -fPIE

clean:
	rm -f libcoreio.so libcoreio.a vsi-ext
