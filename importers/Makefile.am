bin_PROGRAMS = evolution-netscape-importer 	\
	evolution-elm-importer			\
	evolution-pine-importer			\
	evolution-gnomecard-importer

INCLUDES = 						\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DG_LOG_DOMAIN=\"Evolution-Importer\"		\
	-I$(top_srcdir)					\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	$(IMPORTERS_CFLAGS)

IDLS = 			\
	$(top_srcdir)/mail/Mailer.idl

MAIL_GENERATED = 	\
	Mailer.h	\
	Mailer-common.c	\
	Mailer-skels.c	\
	Mailer-stubs.c

$(MAIL_GENERATED): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) `$(GNOME_CONFIG) --cflags idl` $(top_srcdir)/mail/Mailer.idl

evolution_netscape_importer_SOURCES =	\
	$(MAIL_GENERATED)		\
	netscape-importer.c

evolution_netscape_importer_LDADD =				\
	$(top_builddir)/shell/importer/libevolution-importer.la \
	$(top_builddir)/camel/libcamel.la			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/filter/libfilter.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(IMPORTERS_LIBS)

evolution_elm_importer_SOURCES =	\
	elm-importer.c

evolution_elm_importer_LDADD =					\
	$(top_builddir)/shell/importer/libevolution-importer.la \
	$(top_builddir)/e-util/libeutil.la			\
	$(IMPORTERS_LIBS)

evolution_pine_importer_SOURCES =	\
	pine-importer.c

evolution_pine_importer_LDADD =					\
	$(top_builddir)/shell/importer/libevolution-importer.la \
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(IMPORTERS_LIBS)

evolution_gnomecard_importer_SOURCES =	\
	evolution-gnomecard-importer.c

evolution_gnomecard_importer_LDADD =				\
        $(top_builddir)/shell/importer/libevolution-importer.la \
        $(top_builddir)/addressbook/backend/ebook/libebook.la   \
	$(IMPORTERS_LIBS)

oafdir = $(datadir)/oaf
oaf_in_files = GNOME_Evolution_Netscape_Intelligent_Importer.oaf.in	\
	GNOME_Evolution_Elm_Intelligent_Importer.oaf.in			\
	GNOME_Evolution_Pine_Intelligent_Importer.oaf.in		\
	GNOME_Evolution_GnomeCard_Intelligent_Importer.oaf.in

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

EXTRA_DIST =					\
	$(oaf_in_files)				\
	$(oaf_DATA)				\
	ChangeLog.pre-1-4

@XML_I18N_MERGE_OAF_RULE@
