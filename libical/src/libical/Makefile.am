DESIGNDATA = $(top_srcdir)/design-data
ICALSCRIPTS = $(top_srcdir)/scripts

lib_LTLIBRARIES = libical.la

YFLAGS = -d -v -t -pical_yy
LFLAGS = -Pical_yy
LEX_OUTPUT_ROOT = lex.ical_yy

INCLUDES =			\
	-I$(top_builddir)	\
	-I$(top_srcdir)/src	\
	-I$(top_builddir)/src	\
	-I$(srcdir)

libical_la_LDFLAGS = -version-info 0:0:0

libical_la_SOURCES =		\
	icalcomponent.c		\
	icalcomponent.h		\
	icalenums.c		\
	icalenums.h		\
	icalerror.c		\
	icalerror.h		\
	icallexer.l		\
	icalmemory.c		\
	icalmemory.h		\
	icalmime.c		\
	icalmime.h		\
	icalparameter.c		\
	icalparameter.h		\
	icalparser.c		\
	icalparser.h		\
	icalproperty.c		\
	icalproperty.h		\
	icalrecur.c		\
	icalrecur.h		\
	icalrestriction.c	\
	icalrestriction.h	\
	icaltime.c		\
	icaltime.h		\
	icaltypes.c		\
	icaltypes.h		\
	icalvalue.c		\
	icalvalue.h		\
	icalyacc.h		\
	icalyacc.y		\
	pvl.c			\
	pvl.h			\
	sspm.c			\
	sspm.h

libicalincludedir = $(includedir)

libicalinclude_HEADERS =  ical.h 

# ORDERING OF HEADERS IS SIGNIFICANT. Don't change this ordering. It
# is required to make the combined header ical.h properly
COMBINEDHEADERS =				\
	icalversion.h				\
	icaltime.h				\
	icalenums.h				\
	icaltypes.h				\
	icalrecur.h				\
	icalvalue.h				\
	icalparameter.h				\
	icalproperty.h				\
	pvl.h					\
	icalcomponent.h				\
	icalparser.h				\
	icalmemory.h				\
	icalerror.h				\
	icalrestriction.h			\
	sspm.h					\
	icalmime.h

BUILT_SOURCES =			\
	ical.h			\
	icalparameter.c		\
	icalparameter.h		\
	icalproperty.c		\
	icalproperty.h		\
	icalrestriction.c	\
	icalvalue.c		\
	icalvalue.h

ical.h: $(COMBINEDHEADERS)
	cat $(COMBINEDHEADERS)				\
	| egrep -v "#include.*\"ical"			\
	| egrep -v "#include.*\"pvl\.h\"" > ical.h

icallexer.c : icalyacc.h


# parameters

PARAMETERDEPS =	\
	$(ICALSCRIPTS)/mkderivedparameters.pl \
	$(DESIGNDATA)/param-c-types.txt	\
	icalparameter.c.in \
	icalparameter.h.in

icalparameter.h: $(PARAMETERDEPS) 
	$(PERL) $(ICALSCRIPTS)/mkderivedparameters.pl -i $(srcdir)/icalparameter.h.in -h $(DESIGNDATA)/param-c-types.txt > icalparameter.newh \
	&& mv icalparameter.newh icalparameter.h 

icalparameter.c: $(PARAMETERDEPS) icalparameter.h
	$(PERL) $(ICALSCRIPTS)/mkderivedparameters.pl -i $(srcdir)/icalparameter.c.in -c $(DESIGNDATA)/param-c-types.txt > icalparameter.newc \
	&& mv icalparameter.newc icalparameter.c

# properties

PROPERTYDEPS =					\
	$(ICALSCRIPTS)/mkderivedproperties.pl	\
	$(DESIGNDATA)/prop-to-value.txt		\
	$(DESIGNDATA)/value-c-types.txt		\
	icalproperty.c.in			\
	icalproperty.h.in


icalproperty.h: $(PROPERTYDEPS)
	$(PERL) $(ICALSCRIPTS)/mkderivedproperties.pl -i $(srcdir)/icalproperty.h.in -h \
		$(DESIGNDATA)/prop-to-value.txt \
		${DESIGNDATA}/value-c-types.txt > icalproperty.h 

icalproperty.c: $(PROPERTYDEPS) icalproperty.h 
	$(PERL) $(ICALSCRIPTS)/mkderivedproperties.pl -i $(srcdir)/icalproperty.c.in -c \
		$(DESIGNDATA)/prop-to-value.txt \
		${DESIGNDATA}/value-c-types.txt > icalproperty.c

# restrictions

RESTRICTIONDEPS =				\
	$(ICALSCRIPTS)/mkrestrictiontable.pl	\
	$(DESIGNDATA)/restrictions.csv		\
	icalrestriction.c.in

icalrestriction.c: $(RESTRICTIONDEPS)
	$(PERL) $(ICALSCRIPTS)/mkrestrictiontable.pl  -i $(srcdir)/icalrestriction.c.in \
		$(DESIGNDATA)/restrictions.csv > icalrestriction.c

# values

VALUEDEPS =					\
	$(ICALSCRIPTS)/mkderivedvalues.pl  	\
	$(DESIGNDATA)/value-c-types.txt		\
	icalvalue.c.in				\
	icalvalue.h.in

icalvalue.h: $(VALUEDEPS)
	$(PERL) $(ICALSCRIPTS)/mkderivedvalues.pl -i $(srcdir)/icalvalue.h.in -h \
		$(DESIGNDATA)/value-c-types.txt > icalvalue.h

icalvalue.c: $(VALUEDEPS) icalvalue.h
	$(PERL) $(ICALSCRIPTS)/mkderivedvalues.pl  -i $(srcdir)/icalvalue.c.in -c \
		$(DESIGNDATA)/value-c-types.txt > icalvalue.c



# housekeeping
CONFIG_CLEAN_FILES = y.output

CLEANFILES =			\
	icalparameter.h		\
	icalparameter.c		\
	icalproperty.h		\
	icalproperty.c		\
	icalrestriction.c	\
	icalvalue.h		\
	icalvalue.c

EXTRA_DIST =			\
	icalparameter.c.in	\
	icalparameter.h.in	\
	icalproperty.c.in	\
	icalproperty.h.in	\
	icalrestriction.c.in	\
	icalvalue.c.in		\
	icalvalue.h.in		\
	icalversion.h.in	\
	$(BUILT_SOURCES)	\
	icallexer.c		\
	icalyacc.c
