
LIBFILTEROBJS = filter-arg-types.lo filter-arg.lo filter-xml.lo filter-format.lo filter-druid.lo filter-editor.lo
LIBFILTERSRCS = filter-arg-types.c filter-arg.c filter-xml.c filter-format.c filter-druid.c filter-editor.c

LIBTOOL=sh ../libtool
#CC=insure gcc

CFLAGS = `gnome-config --cflags xml gnome gtk gtkhtml gnomeui` -g \
	-I .. -I../libibex -I../camel -I../e-util -I../camel/providers/mbox
LDFLAGS = `gnome-config --libs xml gnome gtk gtkhtml gnomeui` \
	../camel/libcamel.la ../e-util/libeutil.la

all: libfilter.la filter-driver

filter-editor: $(OBJS) filter-editor.lo libfilter.la
	$(LIBTOOL) --mode link $(CC) $^ -o $@ $(LDFLAGS)

filter-driver: $(OBJS) filter-driver.lo libfilter.la
	$(LIBTOOL) --mode link $(CC) $^ -o $@ $(LDFLAGS)

libfilter.la: $(LIBFILTEROBJS)
	$(LIBTOOL) --mode link $(CC) -static $^ -o $@

%.lo: %.c
	$(LIBTOOL) --mode compile $(CC) $(CFLAGS) -c -o $@ $<

install: 
	echo "There is nothing to install"

clean:
	-rm -rf *.o *.lo core .libs
