OBJS=file.lo index.lo find.lo words.lo
MKINDEXOBJS=mkindex.lo
LOOKUPOBJS=lookup.lo

CFLAGS=${PROF} -g -Wall -Wstrict-prototypes -Wmissing-prototypes `glib-config --cflags` `unicode-config --cflags`
LDFLAGS=${PROF}

LIBTOOL=sh ../libtool

all: libibex.la mkindex lookup

%.lo: %.c
	$(LIBTOOL) --mode compile $(CC) $(CFLAGS) -c -o $@ $<

libibex.la: ${OBJS}
	$(LIBTOOL) --mode link $(CC) -static $^ -o $@

mkindex: ${MKINDEXOBJS} libibex.la
	${LIBTOOL) --mode link $(CC) ${LDFLAGS} -o mkindex ${MKINDEXOBJS} libibex.la \
		`glib-config --libs` `unicode-config --libs`

lookup: ${LOOKUPOBJS} libibex.la
	$(LIBTOOL) --mode link ${CC} ${LDFLAGS} -o lookup ${LOOKUPOBJS} libibex.la \
		`glib-config --libs` `unicode-config --libs`

clean:
	rm -f ${OBJS} libibex.la
	rm -f ${MKINDEXOBJS} mkindex
	rm -f ${LOOKUPOBJS} lookup
	rm -f *.core *~ INDEX