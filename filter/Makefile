
OBJS = filter-arg-types.lo filter-arg.lo filter-xml.lo filter-driver.lo filter-format.lo
SRCS = filter-arg-types.c filter-arg.c filter-xml.c filter-driver.c filter-format.c

LIBFILTEROBJS = filter-sexp.lo
LIBFILTERSRCS = filter-sexp.c

LIBTOOL=sh ../libtool

CFLAGS = `gnome-config --cflags xml gnome gtk gtkhtml gnomeui` -g -I../camel -I .. -I../libibex
LDFLAGS = `gnome-config --libs xml gnome gtk gtkhtml gnomeui` ../camel/libcamel.la -lpthread

all: libfilter.la filter-driver

filter-driver: $(OBJS) libfilter.la
	$(LIBTOOL) --mode link $(CC) $^ -o $@ $(LDFLAGS)

libfilter.la: $(LIBFILTEROBJS)
	$(LIBTOOL) --mode link $(CC) -static $^ -o $@

%.lo: %.c
	$(LIBTOOL) --mode compile $(CC) $(CFLAGS) -c -o $@ $<
