
lib_LTLIBRARIES = libical.la

CFLAGS=-g

libical_la_SOURCES =		\
	ical.h			\
	icalcomponent.c		\
	icalcomponent.h		\
	icalenums.c		\
	icalenums.h		\
	icalerror.c		\
	icalerror.h		\
	icalirip.h		\
	icalitipy.y		\
	icalitipl.l		\
	icalmemory.c		\
	icalmemory.h		\
	icalparameter.c		\
	icalparameter.h		\
	icalparser.c		\
	icalparser.h		\
	icalproperty.c		\
	icalproperty.h		\
	icalrestriction.c	\
	icalrestriction.h	\
	icaltypes.c		\
	icaltypes.h		\
	icalvalue.c		\
	icalvalue.h		\
	icalvcal.h		\
	locking.c		\
	pvl.c			\
	pvl.h

EXTRA_DIST = .gdbinit base64.c filelock.c filelock.h

YFLAGS=-picalparser_yy -d
LFLAGS=-Picalparser_yy -olex.yy.c
