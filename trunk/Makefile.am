EXTRA_DIST= impusher.c

all: impusher.so

impusher.so: impusher.c
	$(LIBTOOL) --mode=compile $(CC) $(CFLAGS) -c impusher.c -o impusher.lo $(PIDGIN_CFLAGS)
	$(LIBTOOL) --mode=link    $(CC) $(CFLAGS) -o impusher.la $(LDFLAGS) impusher.lo -rpath /usr/lib/pidgin -module -avoid-version $(PIDGIN_LIBS) 
	cp .libs/impusher.so .

install: impusher.so
	test -d ~/.purple/plugins || mkdir -p ~/.purple/plugins
	cp impusher.so ~/.purple/plugins

clean:
	rm -rf *.o *.la *.lo
