noinst_PROGRAMS = test-recur

INCLUDES =						\
	-DG_LOG_DOMAIN=\"cal-util\"			\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/calendar			\
	-I. 						\
	-I.. 						\
	-I$(top_builddir)				\
	-I$(top_srcdir)/libical/src			\
	$(EVOLUTION_CALENDAR_CFLAGS)

#
# cal util library
#

privlib_LTLIBRARIES = libcal-util.la

libcal_util_la_SOURCES = 	\
	cal-util-marshal.c	\
	cal-util-marshal.h	\
	cal-component.c		\
	cal-recur.c		\
	cal-util.c		\
	timeutil.c

libcal_util_la_LIBADD =		\
	 $(top_builddir)/libical/src/libical/libical-evolution.la

libcal_utilincludedir = $(privincludedir)/cal-util

libcal_utilinclude_HEADERS =	\
	cal-component.h		\
	cal-recur.h		\
	cal-util.h		\
	timeutil.h

MARSHAL_GENERATED = cal-util-marshal.c cal-util-marshal.h
@EVO_MARSHAL_RULE@

BUILT_SOURCES = $(MARSHAL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

test_recur_SOURCES = 	\
	 test-recur.c

test_recur_LDADD = 							\
	 libcal-util.la							\
	 $(EVOLUTION_CALENDAR_LIBS)

EXTRA_DIST = \
	cal-util-marshal.list
