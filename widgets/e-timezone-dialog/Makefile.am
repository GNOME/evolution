INCLUDES =					\
	-I$(top_srcdir)				\
	$(EXTRA_GNOME_CFLAGS)			\
	-DMAP_DIR=\""$(datadir)/images/evolution"\"	\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DG_LOG_DOMAIN=__FILE__

noinst_LIBRARIES =	\
	libetimezonedialog.a

libetimezonedialog_a_SOURCES = 	\
	e-timezone-dialog.c	\
	e-timezone-dialog.h

gladedir = $(datadir)/evolution/glade
glade_DATA = e-timezone-dialog.glade

EXTRA_DIST = $(glade_DATA)
