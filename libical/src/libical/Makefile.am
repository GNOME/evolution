
#AUTOMAKE_OPTIONS = no-dependencies

#noinst_LTLIBRARIES = libical.la
lib_LTLIBRARIES = libical.la

INCLUDES = -I.

YFLAGS =-d -v -t

libical_la_SOURCES = \
	ical.h \
	icalcomponent.c	\
	icalcomponent.h	\
	icalenums.c \
	icalenums.h \
	icalerror.c \
	icalerror.h \
	icalyacc.y \
	icallexer.l \
	icalmemory.c \
	icalmemory.h \
	icalparameter.c \
	icalparameter.h \
	icalparser.c \
	icalparser.h \
	icalproperty.c \
	icalproperty.h \
	icalrestriction.c \
	icalrestriction.h \
	icaltypes.c \
	icaltypes.h \
	icalvalue.c \
	icalvalue.h \
	icalvcal.h \
	pvl.c \
	pvl.h

include_HEADERS=\
	ical.h \
	icalcomponent.h	\
	icalenums.h \
	icalerror.h \
	icalmemory.h \
	icalparameter.h \
	icalparser.h \
	icalproperty.h \
	icalrestriction.h \
	icaltypes.h \
	icalvalue.h \
	icalvcal.h \
	icalversion.h \
	pvl.h



EXTRA_DIST = icallexer.c icalyacc.c icalyacc.h

CONFIG_CLEAN_FILES =  y.output *~ 

DESIGNDATA = ../../design-data
ICALSCRIPTS = ../../scripts
derived: icalproperty icalparameter icalvalue

icalproperty:
	$(ICALSCRIPTS)/mkderivedproperties.pl -i icalproperty.h -h $(DESIGNDATA)/prop-to-value.txt ${DESIGNDATA}/value-c-types.txt > icalproperty.newh
	mv icalproperty.newh icalproperty.h

	$(ICALSCRIPTS)/mkderivedproperties.pl -i icalproperty.c -c $(DESIGNDATA)/prop-to-value.txt ${DESIGNDATA}/value-c-types.txt > icalproperty.newc
	mv icalproperty.newc icalproperty.c


icalparameter: 
	$(ICALSCRIPTS)/mkderivedparameters.pl -i icalparameter.h -h $(DESIGNDATA)/param-c-types.txt > icalparameter.newh
	mv icalparameter.newh icalparameter.h

	$(ICALSCRIPTS)/mkderivedparameters.pl -i icalparameter.c -c $(DESIGNDATA)/param-c-types.txt > icalparameter.newc
	mv icalparameter.newc icalparameter.c

icalvalue: 
	$(ICALSCRIPTS)/mkderivedvalues.pl -i icalvalue.h -h $(DESIGNDATA)/value-c-types.txt > icalvalue.newh
	mv icalvalue.newh icalvalue.h

	$(ICALSCRIPTS)/mkderivedvalues.pl  -i icalvalue.c -c $(DESIGNDATA)/value-c-types.txt > icalvalue.newc
	mv icalvalue.newc icalvalue.c


