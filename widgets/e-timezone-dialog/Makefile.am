INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/libical/src/libical				\
	-I$(top_builddir)/libical/src/libical				\
	-DMAP_DIR=\""$(datadir)/evolution-$(BASE_VERSION)/images"\"	\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"				\
	-DG_LOG_DOMAIN=__FILE__						\
	$(GNOME_FULL_CFLAGS)

noinst_LIBRARIES =	\
	libetimezonedialog.a

libetimezonedialog_a_SOURCES = 	\
	e-timezone-dialog.c	\
	e-timezone-dialog.h

gladedir = $(datadir)/evolution-$(BASE_VERSION)/glade
glade_DATA = e-timezone-dialog.glade

EXTRA_DIST = $(glade_DATA)
