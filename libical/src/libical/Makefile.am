
#AUTOMAKE_OPTIONS = no-dependencies

#noinst_LTLIBRARIES = libical.la
lib_LIBRARIES = libical.a

YFLAGS =-d -v -t

libical_a_SOURCES = \
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
	icalrecur.c \
	icalrecur.h \
	icaltime.h \
	icaltime.c \
	pvl.c \
	pvl.h \
	sspm.c \
	sspm.h \
	icalmime.c \
	icalmime.h 

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
	icalrecur.h \
	icalversion.h \
	icaltime.h \
	pvl.h \
	sspm.h \
	icalmime.h 


#
# static convenience library for use in conduits
#

noinst_LTLIBRARIES = libical-static.la
libical_static_la_SOURCES = $(libical_a_SOURCES)
libical_static_la_LDFLAGS = --all-static

EXTRA_DIST = icallexer.c icalyacc.c icalyacc.h

CONFIG_CLEAN_FILES =  y.output *~ 

DESIGNDATA = ../../design-data
ICALSCRIPTS = ../../scripts
derived: icalproperty icalparameter icalvalue icalrestriction

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

icalrestriction: 
	$(ICALSCRIPTS)/mkrestrictiontable.pl  -i icalrestriction.c  $(DESIGNDATA)/restrictions.csv > icalrestriction.newc
	mv icalrestriction.newc icalrestriction.c


