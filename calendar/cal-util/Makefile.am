noinst_PROGRAMS = test-recur

INCLUDES =						\
	-DG_LOG_DOMAIN=\"cal-util\"			\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/calendar			\
	-I. 						\
	-I.. 						\
	-I$(top_builddir)				\
	-I$(top_srcdir)/libical/src/libical		\
	-I$(top_builddir)/libical/src/libical		\
	$(EVOLUTION_CALENDAR_CFLAGS)

#
# cal util library
#

privlib_LTLIBRARIES = libcal-util-static.la libcal-util.la

libcal_util_la_SOURCES = 	\
	cal-util-marshal.c	\
	cal-util-marshal.h	\
	cal-component.c		\
	cal-recur.c		\
	cal-util.c		\
	timeutil.c

libcal_utilincludedir = $(privincludedir)/cal-util

libcal_utilinclude_HEADERS =	\
	cal-component.h		\
	cal-recur.h		\
	cal-util.h		\
	timeutil.h

# GLib marshalling cruft

cal-util-marshal.h: cal-util-marshal.list
	( @GLIB_GENMARSHAL@ --prefix=cal_util_marshal cal-util-marshal.list --header > cal-util-marshal.tmp \
	&& mv cal-util-marshal.tmp cal-util-marshal.h ) \
	|| ( rm -f cal-util-marshal.tmp && exit 1 )

cal-util-marshal.c: cal-util-marshal.h
	( @GLIB_GENMARSHAL@ --prefix=cal_util_marshal cal-util-marshal.list --body > cal-util-marshal.tmp \
	&& mv cal-util-marshal.tmp cal-util-marshal.c ) \
	|| ( rm -f cal-util-marshal.tmp && exit 1 )

MARSHAL_GENERATED = cal-util-marshal.c cal-util-marshal.h

BUILT_SOURCES = $(MARSHAL_GENERATED)

#
# static library for use in conduits' shared libraries
#
#lib_LTLIBRARIES = libcal-util-static.la
libcal_util_static_la_SOURCES = $(libcal_util_la_SOURCES)
libcal_util_static_la_LDFLAGS = -all-static

test_recur_SOURCES = 	\
	 test-recur.c

test_recur_LDADD = 							\
	 libcal-util.la							\
	 $(top_builddir)/libical/src/libical/libical-evolution.la	\
	 $(EVOLUTION_CALENDAR_LIBS)

EXTRA_DIST = \
	cal-util-marshal.list