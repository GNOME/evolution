INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/libical/src					\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"				\
	-DG_LOG_DOMAIN=__FILE__						\
	$(GNOME_FULL_CFLAGS)

noinst_LTLIBRARIES =	\
	libetimezonedialog.la

libetimezonedialog_la_SOURCES =	\
	e-timezone-dialog.c	\
	e-timezone-dialog.h

glade_DATA = e-timezone-dialog.glade

EXTRA_DIST = $(glade_DATA)
