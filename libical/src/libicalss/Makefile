# Generated automatically from Makefile.in by configure.
SOURCES = icalcluster.c icalstore.c icalcalendar.c
OBJECTS =  ${SOURCES:.c=.o}
INCLUDES = -I . -I.. -I../libical
CFLAGS =  -Wall -g  $(INCLUDES)  -DSTDC_HEADERS=1 -DHAVE_MALLOC_H=1 -DHAVE_UNISTD_H=1 -DHAVE_TIME_H=1 -DHAVE_STDIO_H=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_ASSERT_H=1 -DHAVE_STRDUP=1  -DICAL_ERRORS_ARE_FATAL 
LIBS   = 
CC     = gcc
AR     = ar cr
RANLIB = ranlib

prefix = /usr/local
PREFIX = ${prefix}
EXEC_PREFIX = ${prefix}
BIN_DIR = $(EXEC_PREFIX)/bin
LIB_DIR = $(EXEC_PREFIX)/lib
INCLUDE_DIR = $(PREFIX)/include

INSTALL        = /usr/bin/install -c
INSTALL_DATA   = ${INSTALL} -m 644
INSTALL_PROGRAM= ${INSTALL} -m 755       

all: lib

lib: $(OBJECTS) 
	$(AR) libicalss.a $(OBJECTS)
	$(RANLIB) libicalss.a


clean:
	-/bin/rm -f *.o *.a
	-/bin/rm -f \#*
	-/bin/rm -f *~ Makefile.bak icalitip.tab.h icalitip.tab.c lex.yy.c
	-/bin/rm -f icaltest parser core output *.output

ci: clean
	ci -u *.c *.h *.l *.y Makefile.in

depend:
	@makedepend  -Y $(INCLUDES) $(SOURCES)


install:
	$(INSTALL_DATA) libicalss.a $(LIB_DIR)/libicalss.a;
	for i in `ls *.h`;  \
	do \
		$(INSTALL_DATA)  $$i $(INCLUDE_DIR)/$$i; \
	done;


