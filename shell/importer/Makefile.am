INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/shell	\
	-I$(includedir) $(GNOME_INCLUDEDIR) 		\
	-DG_LOG_DOMAIN=\"Evolution-Importer\" 		\
	-DEVOLUTION_GLADEDIR=\"$(datadir)/evolution/glade\"

lib_LTLIBRARIES = libevolution-importer.la

IDLS = \
	GNOME_Evolution_Importer.idl

IDL_GENERATED = 	\
	GNOME_Evolution_Importer.h	\
	GNOME_Evolution_Importer-common.c	\
	GNOME_Evolution_Importer-skels.c	\
	GNOME_Evolution_Importer-stubs.c

GNOME_Evolution_Importer-impl.o: GNOME_Evolution_Importer.h

$(IDL_GENERATED): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl`	\
		$(srcdir)/GNOME_Evolution_Importer.idl

idldir = $(datadir)/idl
idl_DATA = $(IDLS)

libevolution_importerincludedir = $(includedir)/evolution/importer
libevolution_importer_la_SOURCES = 	\
	$(IDL_GENERATED)		\
	evolution-importer-client.c	\
	evolution-importer-listener.c	\
	evolution-importer.c		\
	importer.c			\
	importer.h

libevolution_importerinclude_HEADERS = 	\
	GNOME_Evolution_Importer.h	\
	evolution-importer-client.h	\
	evolution-importer-listener.h	\
	evolution-importer.h

libevolution_importer_la_LIBADD =	\
	$(top_builddir)/e-util/libeutil.la	\
	$(EXTRA_GNOME_LIBS)		\
	$(GTKHTML_LIBS)

gladedir = $(datadir)/evolution/glade
glade_DATA = import.glade

EXTRA_DIST = $(glade_DATA) $(IDLS)
