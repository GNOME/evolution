INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/libical/src/libical				\
	-I$(top_builddir)/libical/src/libical				\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"				\
	-DG_LOG_DOMAIN=__FILE__						\
	$(GNOME_FULL_CFLAGS)

privlib_LTLIBRARIES =		\
	libetimezonedialog.la

libetimezonedialog_la_SOURCES =	\
	e-timezone-dialog.c	\
	e-timezone-dialog.h

glade_DATA = e-timezone-dialog.glade

EXTRA_DIST = $(glade_DATA)
