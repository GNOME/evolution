OBJS=file.o index.o find.o words.o
MKINDEXOBJS=mkindex.o
LOOKUPOBJS=lookup.o

CFLAGS=${PROF} -g -Wall -Wstrict-prototypes -Wmissing-prototypes `glib-config --cflags` `unicode-config --cflags`
LDFLAGS=${PROF}

all: libibex.a mkindex lookup

libibex.a: ${OBJS}
	ar cru $@ ${OBJS}
	@test -x /usr/bin/ranlib && ranlib $@

mkindex: ${MKINDEXOBJS} libibex.a
	${CC} ${LDFLAGS} -o mkindex ${MKINDEXOBJS} libibex.a \
		`glib-config --libs` `unicode-config --libs`

lookup: ${LOOKUPOBJS} libibex.a
	${CC} ${LDFLAGS} -o lookup ${LOOKUPOBJS} libibex.a \
		`glib-config --libs` `unicode-config --libs`

clean:
	rm -f ${OBJS} libibex.a
	rm -f ${MKINDEXOBJS} mkindex
	rm -f ${LOOKUPOBJS} lookup
	rm -f *.core *~ INDEX