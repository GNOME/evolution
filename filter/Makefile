
OBJS = filter-arg-types.lo filter-arg.lo filter-xml.lo filter-format.lo filter-druid.lo
SRCS = filter-arg-types.c filter-arg.c filter-xml.c filter-format.c filter-druid.c

LIBFILTEROBJS = filter-sexp.lo
LIBFILTERSRCS = filter-sexp.c

LIBTOOL=sh ../libtool
#CC=insure gcc

CFLAGS = `gnome-config --cflags xml gnome gtk gtkhtml gnomeui` -g -I../camel -I .. -I../libibex
LDFLAGS = `gnome-config --libs xml gnome gtk gtkhtml gnomeui` ../camel/libcamel.la -lpthread

all: libfilter.la filter-editor

filter-editor: $(OBJS) filter-editor.lo libfilter.la
	$(LIBTOOL) --mode link $(CC) $^ -o $@ $(LDFLAGS)

filter-driver: $(OBJS) filter-driver.lo libfilter.la
	$(LIBTOOL) --mode link $(CC) $^ -o $@ $(LDFLAGS)

libfilter.la: $(LIBFILTEROBJS)
	$(LIBTOOL) --mode link $(CC) -static $^ -o $@

%.lo: %.c
	$(LIBTOOL) --mode compile $(CC) $(CFLAGS) -c -o $@ $<

clean:
	-rm -rf *.o *.lo core .libs
