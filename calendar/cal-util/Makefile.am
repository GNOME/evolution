
INCLUDES =						\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DG_LOG_DOMAIN=\"cal-client\"			\
	-I$(srcdir) -I$(top_srcdir) 			\
	-I. 						\
	-I.. 						\
	-I$(top_builddir)				\
        -I$(includedir)                                 \
        $(GNOME_INCLUDEDIR)

lib_LTLIBRARIES = libcalutil.la

libcalutil_la_SOURCES = \
	cal-util.c	\
	cal-util.h	\
	calobj.c	\
	calobj.h	\
	timeutil.c	\
	timeutil.h

