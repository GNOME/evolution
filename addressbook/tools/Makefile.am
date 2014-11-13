privlibexec_SCRIPTS =					\
	csv2vcard

privlibexec_PROGRAMS = evolution-addressbook-export

evolution_addressbook_export_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-DG_LOG_DOMAIN=\"evolution-addressbook-tools\"	\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBDIR=\""$(libdir)"\"			\
	-I$(top_srcdir)/addressbook			\
	-I$(top_builddir)/addressbook			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CODE_COVERAGE_CFLAGS)

evolution_addressbook_export_SOURCES =				\
	evolution-addressbook-export.c				\
	evolution-addressbook-export-list-cards.c 		\
	evolution-addressbook-export-list-folders.c		\
	evolution-addressbook-export.h

evolution_addressbook_export_LDADD =				\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

evolution_addressbook_export_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

if OS_WIN32
evolution_addressbook_export_LDFLAGS += -mwindows
endif

EXTRA_DIST = $(privlibexec_SCRIPTS)

-include $(top_srcdir)/git.mk
