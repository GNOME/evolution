importersdir = $(privlibdir)/evolution-mail-importers

importers_LTLIBRARIES = liboutlook.la libmbox.la \
	libevolution-elm-importer.la	\
	libevolution-pine-importer.la	\
	libevolution-netscape-importer.la

INCLUDES = -I.. 					\
	-I$(srcdir)/..					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/shell				\
	-I$(top_builddir)/shell				\
	-DG_LOG_DOMAIN=\"evolution-mail-importer\"	\
	-I$(top_srcdir)/addressbook/backend             \
	-I$(top_builddir)/addressbook/backend           \
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
	$(IMPORTERS_CFLAGS)

liboutlook_la_SOURCES =				\
	evolution-outlook-importer.c
liboutlook_la_LDFLAGS = -avoid-version -module

libmbox_la_SOURCES = evolution-mbox-importer.c \
		     mozilla-status-headers.h
libmbox_la_LDFLAGS = -avoid-version -module

IDLS = 			\
	$(top_srcdir)/mail/Mailer.idl

MAIL_GENERATED_H = 	\
	Mailer.h
MAIL_GENERATED_C = 	\
	Mailer-common.c	\
	Mailer-skels.c	\
	Mailer-stubs.c
MAIL_GENERATED = $(MAIL_GENERATED_C) $(MAIL_GENERATED_H)

$(MAIL_GENERATED_H): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) $(top_srcdir)/mail/Mailer.idl
$(MAIL_GENERATED_C): $(MAIL_GENERATED_H)

libevolution_netscape_importer_la_SOURCES =	\
	$(MAIL_GENERATED)		\
	netscape-importer.c

libevolution_netscape_importer_la_LDFLAGS = -avoid-version -module

libevolution_netscape_importer_la_LIBADD =			\
	$(top_builddir)/shell/importer/libevolution-importer.la \
	$(top_builddir)/camel/libcamel.la			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/filter/libfilter.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(IMPORTERS_LIBS)

libevolution_elm_importer_la_SOURCES =	\
	elm-importer.c

libevolution_elm_importer_la_LDFLAGS = -avoid-version -module

libevolution_elm_importer_la_LIBADD =				\
	$(top_builddir)/shell/importer/libevolution-importer.la \
	$(top_builddir)/e-util/libeutil.la			\
	$(IMPORTERS_LIBS)

libevolution_pine_importer_la_SOURCES =	\
	pine-importer.c

libevolution_pine_importer_la_LDFLAGS = -avoid-version -module

libevolution_pine_importer_la_LIBADD =				\
	$(top_builddir)/shell/importer/libevolution-importer.la \
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/camel/libcamel.la			\
	$(IMPORTERS_LIBS)

# evolution_gnomecard_importer_SOURCES =	\
# 	evolution-gnomecard-importer.c

# evolution_gnomecard_importer_LDADD =				\
#         $(top_builddir)/shell/importer/libevolution-importer.la \
#         $(top_builddir)/addressbook/backend/ebook/libebook.la   \
# 	$(top_builddir)/camel/libcamel.la			\
# 	$(IMPORTERS_LIBS)


server_in_files = GNOME_Evolution_Mail_Mbox_Importer.server.in.in	\
	GNOME_Evolution_Mail_Outlook_Importer.server.in.in		\
	GNOME_Evolution_Mail_Netscape_Intelligent_Importer.server.in.in	\
	GNOME_Evolution_Mail_Elm_Intelligent_Importer.server.in.in	\
	GNOME_Evolution_Mail_Pine_Intelligent_Importer.server.in.in
server_DATA = $(server_in_files:.server.in.in=.server)
%.server.in: %.server.in.in
	sed -e "s|\@BINDIR\@|$(bindir)|"				\
	    -e "s|\@VERSION\@|$(BASE_VERSION)|"				\
	    $< > $@

EXTRA_DIST = $(server_in_files) $(server_DATA)

BUILT_SOURCES = $(MAIL_GENERATED) $(server_DATA)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

@INTLTOOL_SERVER_RULE@
