#noinst_PROGRAMS = test-recur

INCLUDES =						\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DG_LOG_DOMAIN=\"cal-util\"			\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/calendar			\
	-I. 						\
	-I.. 						\
	-I$(top_builddir)				\
	-I$(top_srcdir)/libical/src/libical		\
	-I$(top_builddir)/libical/src/libical		\
        $(GNOME_INCLUDEDIR)

#
# cal util library
#

lib_LTLIBRARIES = libcal-util.la

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
noinst_LTLIBRARIES = libcal-util-static.la
libcal_util_static_la_SOURCES = $(libcal_util_la_SOURCES)
libcal_util_static_la_LDFLAGS = --all-static

#test_recur_SOURCES = 	\
#	 test-recur.c
#
#test_recur_LDADD = 	\
#	 libcal-util.la	\
#	 $(top_builddir)/libversit/libversit.la		\
#	 $(top_builddir)/libical/src/libical/libical.a	\
#	 $(EXTRA_GNOME_LIBS)
