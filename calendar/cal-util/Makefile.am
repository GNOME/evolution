noinst_PROGRAMS = test-recur

INCLUDES =						\
        -DGNOMELOCALEDIR=\""$(localedir)"\"		\
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

lib_LTLIBRARIES = libcal-util.la
privlib_LTLIBRARIES = libcal-util-static.la

libcal_util_la_SOURCES = 	\
	cal-component.c		\
	cal-recur.c		\
	cal-util.c		\
	timeutil.c

libcal_utilincludedir = $(includedir)/evolution/cal-util

libcal_utilinclude_HEADERS =	\
	cal-component.h		\
	cal-recur.h		\
	cal-util.h		\
	timeutil.h

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
	 $(top_builddir)/libversit/libversit.a				\
	 $(top_builddir)/libical/src/libical/libical-evolution.la	\
	 $(EVOLUTION_CALENDAR_LIBS)
