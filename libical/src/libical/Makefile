# Generated automatically from Makefile.in by configure.
DESIGNDATA = ../../design-data
SCRIPTS = ../../scripts
SOURCES = icalcomponent.c icalproperty.c icalparameter.c icalenums.c icalvalue.c icalerror.c icalmemory.c icaltypes.c icalparser.c icalrestriction.c pvl.c
OBJECTS =  ${SOURCES:.c=.o}
INCLUDES = -I . -I..
CFLAGS =  -Wall -g -O2  $(INCLUDES)  -DSTDC_HEADERS=1 -DHAVE_MALLOC_H=1 -DHAVE_UNISTD_H=1 -DHAVE_TIME_H=1 -DHAVE_STDIO_H=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_ASSERT_H=1 -DHAVE_STRDUP=1  -DICAL_ERRORS_ARE_FATAL 
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

test: $(OBJECTS) usecases.o
	$(CC)  -o icaltest $(OBJECTS) usecases.o $(INCLUDES)  $(LIBS)
	icaltest

icalitip.tab.c: icalitip.y
	bison -d -v -t icalitip.y

lex.yy.c: icalitip.l
	flex -s  icalitip.l


lib: icalitip.tab.o lex.yy.o $(OBJECTS) 
	$(AR) libical.a icalitip.tab.o lex.yy.o $(OBJECTS)
	$(RANLIB) libical.a


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
	$(INSTALL_DATA) libical.a $(LIB_DIR)/libical.a;
	for i in `ls *.h`;  \
	do \
		$(INSTALL_DATA)  $$i $(INCLUDE_DIR)/$$i; \
	done;


derived: icalproperty icalparameter icalvalue

icalproperty:
	$(SCRIPTS)/mkderivedproperties.pl -i icalproperty.h -h $(DESIGNDATA)/prop-to-value.txt ${DESIGNDATA}/value-c-types.txt > icalproperty.newh
	mv icalproperty.newh icalproperty.h

	$(SCRIPTS)/mkderivedproperties.pl -i icalproperty.c -c $(DESIGNDATA)/prop-to-value.txt ${DESIGNDATA}/value-c-types.txt > icalproperty.newc
	mv icalproperty.newc icalproperty.c


icalparameter: 
	$(SCRIPTS)/mkderivedparameters.pl -i icalparameter.h -h $(DESIGNDATA)/param-c-types.txt > icalparameter.newh
	mv icalparameter.newh icalparameter.h

	$(SCRIPTS)/mkderivedparameters.pl -i icalparameter.c -c $(DESIGNDATA)/param-c-types.txt > icalparameter.newc
	mv icalparameter.newc icalparameter.c

icalvalue: 
	$(SCRIPTS)/mkderivedvalues.pl -i icalvalue.h -h $(DESIGNDATA)/value-c-types.txt > icalvalue.newh
	mv icalvalue.newh icalvalue.h

	$(SCRIPTS)/mkderivedvalues.pl  -i icalvalue.c -c $(DESIGNDATA)/value-c-types.txt > icalvalue.newc
	mv icalvalue.newc icalvalue.c


#icalderivedcomponents.h: $(DESIGNDATA)/components.txt $(SCRIPTS)/mkderivedcomponents.pl
#	$(SCRIPTS)/mkderivedcomponents.pl -h $(DESIGNDATA)/components.txt > icalderivedcomponents.h

#icalderivedcomponents.inc: $(DESIGNDATA)/components.txt $(SCRIPTS)/mkderivedcomponents.pl
#	$(SCRIPTS)/mkderivedcomponents.pl -c $(DESIGNDATA)/components.txt > icalderivedcomponents.inc

# DO NOT DELETE

icalcomponent.o: ical.h icalenums.h icalvalue.h icaltypes.h icalparameter.h
icalcomponent.o: icalproperty.h icalcomponent.h icalerror.h
icalcomponent.o: icalmemory.h

icalproperty.o: ical.h icalenums.h icalvalue.h icaltypes.h icalparameter.h
icalproperty.o: icalproperty.h icalcomponent.h icalerror.h
icalproperty.o: icalmemory.h

icalparameter.o: ical.h icalenums.h icalvalue.h icaltypes.h icalparameter.h
icalparameter.o: icalproperty.h icalcomponent.h icalerror.h icalmemory.h

icalenums.o: icalenums.h

icalvalue.o: ical.h icalenums.h icalvalue.h icaltypes.h icalparameter.h
icalvalue.o: icalproperty.h icalcomponent.h icalerror.h icalmemory.h
icalvalue.o: icalparser.h
icalerror.o: icalerror.h

icalmemory.o: icalmemory.h icalerror.h

icaltypes.o: icaltypes.h icalerror.h

icalparser.o: ical.h icalenums.h icalvalue.h icaltypes.h icalparameter.h
icalparser.o: icalproperty.h icalcomponent.h icalparser.h
icalparser.o: icalmemory.h
