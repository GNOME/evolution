bin_PROGRAMS = evolution-netscape-importer 	\
	evolution-elm-importer			\
	evolution-pine-importer

INCLUDES = 				\
	-DGNOMELOCALEDIR=\""$(localedir)"\"	\
	-DG_LOG_DOMAIN=\"Evolution-Importer\"	\
	-I$(top_srcdir)			\
	-I$(top_builddir)/shell			\
	-I$(top_srcdir)/shell			\
	-I$(top_srcdir)/addressbook/backend/ebook	\
	$(BONOBO_GNOME_CFLAGS)			\
	$(EXTRA_GNOME_CFLAGS)

IDLS = 			\
	$(top_srcdir)/mail/Mail.idl

MAIL_GENERATED = 	\
	Mail.h		\
	Mail-common.c	\
	Mail-skels.c	\
	Mail-stubs.c

$(MAIL_GENERATED): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) `$(GNOME_CONFIG) --cflags idl` $(top_srcdir)/mail/Mail.idl

evolution_netscape_importer_SOURCES =	\
	$(MAIL_GENERATED)		\
	netscape-importer.c

evolution_netscape_importer_LDADD =	\
	$(BONOBO_GNOME_LIBS)		\
	$(EXTRA_GNOME_LIBS)		\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/shell/importer/libevolution-importer.la

evolution_elm_importer_SOURCES =	\
	elm-importer.c

evolution_elm_importer_LDADD =		\
	$(BONOBO_GNOME_LIBS)		\
	$(EXTRA_GNOME_LIBS)		\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/shell/importer/libevolution-importer.la

evolution_pine_importer_SOURCES =	\
	pine-importer.c

evolution_pine_importer_LDADD =		\
	$(BONOBO_GNOME_LIBS)		\
	$(EXTRA_GNOME_LIBS)		\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/e-util/ename/libename.la	\
	$(top_builddir)/libversit/libversit.la	\
	$(top_builddir)/shell/importer/libevolution-importer.la

oafdir = $(datadir)/oaf
oaf_in_files = GNOME_Evolution_Netscape_Intelligent_Importer.oaf.in	\
	GNOME_Evolution_Elm_Intelligent_Importer.oaf.in			\
	GNOME_Evolution_Pine_Intelligent_Importer.oaf.in

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA)

@XML_I18N_MERGE_OAF_RULE@
