
noinst_PROGRAMS = test-recur

INCLUDES =						\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DG_LOG_DOMAIN=\"cal-util\"			\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/calendar			\
	-I. 						\
	-I.. 						\
	-I$(top_builddir)				\
        $(GNOME_INCLUDEDIR)

lib_LTLIBRARIES = libcal-util.la

libcal_util_la_SOURCES = \
	cal-recur.c	\
	cal-util.c	\
	calobj.c	\
	timeutil.c

libcal_utilincludedir = $(includedir)/evolution/cal-util

libcal_utilinclude_HEADERS =	\
	cal-recur.h		\
	cal-util.h		\
	calobj.h		\
	timeutil.h


test_recur_SOURCES = 	\
	test-recur.c

test_recur_LDADD = 	\
	libcal-util.la	\
	$(top_builddir)/libversit/libversit.la		\
	$(top_builddir)/libical/src/libical/libical.la	\
	$(EXTRA_GNOME_LIBS)
