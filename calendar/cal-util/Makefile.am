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
