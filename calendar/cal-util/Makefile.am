
noinst_PROGRAMS = test-recur icalendar-test

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

lib_LTLIBRARIES = libcal-util.la

libcal_util_la_SOURCES = 	\
	cal-recur.c		\
	cal-util.c		\
	calobj.c		\
	icalendar.c		\
	icalendar.h		\
	icalendar-save.c	\
	icalendar-save.h	\
	timeutil.c

libcal_utilincludedir = $(includedir)/evolution/cal-util

libcal_utilinclude_HEADERS =	\
	cal-recur.h		\
	cal-util.h		\
	calobj.h		\
	timeutil.h

icalendar_test_SOURCES =	\
	icalendar-test.c	\
	icalendar.c		\
	icalendar-save.c

icalendar_test_LDADD =						\
	$(EXTRA_GNOME_LIBS)					\
	$(top_builddir)/libversit/libversit.la			\
	$(top_builddir)/libical/src/libical/libical.a

#	$(top_builddir)/calendar/cal-util/libcal-util.la	\

test_recur_SOURCES = 	\
	test-recur.c

test_recur_LDADD = 	\
	libcal-util.la	\
	$(top_builddir)/libversit/libversit.la		\
	$(top_builddir)/libical/src/libical/libical.a	\
	$(EXTRA_GNOME_LIBS)
