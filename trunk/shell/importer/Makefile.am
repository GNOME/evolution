INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/shell			\
	-I$(top_builddir)/shell			\
	-DG_LOG_DOMAIN=\"Evolution-Importer\"	\
	-DEVOLUTION_GLADEDIR="\"$(gladedir)\""	\
	$(SHELL_CFLAGS)

privlib_LTLIBRARIES = libevolution-importer.la


# IDL stuff

IDLS = GNOME_Evolution_Importer.idl

IDL_GENERATED_H = 			\
	GNOME_Evolution_Importer.h

IDL_GENERATED_C =				\
	GNOME_Evolution_Importer-common.c	\
	GNOME_Evolution_Importer-skels.c	\
	GNOME_Evolution_Importer-stubs.c

IDL_GENERATED = $(IDL_GENERATED_H) $(IDL_GENERATED_C)

$(IDL_GENERATED_H): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) $(srcdir)/GNOME_Evolution_Importer.idl

$(IDL_GENERATED_C): $(IDL_GENERATED_H)

idl_DATA = $(IDLS)


# Component

libevolution_importerincludedir = $(privincludedir)/importer
libevolution_importer_la_SOURCES = 		\
	$(IDL_GENERATED)			\
	evolution-intelligent-importer.c	\
	evolution-importer-client.c		\
	evolution-importer-listener.c		\
	evolution-importer.c			\
	intelligent.c				\
	intelligent.h

libevolution_importerinclude_HEADERS = 		\
	GNOME_Evolution_Importer.h		\
	evolution-intelligent-importer.h	\
	evolution-importer-client.h		\
	evolution-importer-listener.h		\
	evolution-importer.h

libevolution_importer_la_LIBADD =		\
	$(top_builddir)/e-util/libeutil.la 	\
	$(SHELL_LIBS)

glade_DATA = import.glade

EXTRA_DIST = $(glade_DATA) $(IDLS)

BUILT_SOURCES= $(IDL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

