INCLUDES =						\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DG_LOG_DOMAIN=\"cal-util\"			\
	-I$(srcdir) -I$(top_srcdir) 			\
	-I$(srcdir)/calendar				\
	-I. 						\
	-I.. 						\
	-I$(top_builddir)				\
        -I$(includedir)                                 \
        $(GNOME_INCLUDEDIR)

lib_LTLIBRARIES = libcal-util.la

libcal_util_la_SOURCES = \
	cal-util.c	\
	calobj.c	\
	timeutil.c

libcal_utilincludedir = $(includedir)/evolution/cal-util

libcal_utilinclude_HEADERS =	\
	cal-util.h		\
	calobj.h		\
	timeutil.h
