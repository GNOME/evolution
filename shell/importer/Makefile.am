INCLUDES = -I$(top_srcdir) -I$(includedir) $(GNOME_INCLUDEDIR) \
	-DG_LOG_DOMAIN=\"Evolution-Importer\" -g -Wall \
	-Wmissing-prototypes -Wmissing-declarations	\
	-DEVOLUTION_GLADEDIR=\"$(datadir)/evolution/glade\"

lib_LTLIBRARIES = libevolution-importer.la

noinst_LIBRARIES = libimporter.a

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

libimporter_a_SOURCES =		\
	importer.c		\
	importer.h

libevolution_importerincludedir = $(includedir)/evolution/importer
libevolution_importer_la_SOURCES = 	\
	$(IDL_GENERATED)		\
	evolution-importer-factory.c	\
	evolution-importer-client.c	\
	evolution-importer.c

libevolution_importerinclude_HEADERS = 	\
	evolution-importer-factory.h	\
	evolution-importer-client.h	\
	evolution-importer.h

gladedir = $(datadir)/evolution/glade
glade_DATA = import.glade

EXTRA_DIST = $(glade_DATA) $(oaf_DATA)
