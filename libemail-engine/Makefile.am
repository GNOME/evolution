NULL =

privsolib_LTLIBRARIES = libemail-engine.la

include $(top_srcdir)/glib-gen.mak
glib_enum_headers=e-mail-enums.h
glib_enum_output=e-mail-enumtypes
glib_enum_define=E_MAIL
glib_enum_prefix=e_mail

ENUM_GENERATED = e-mail-enumtypes.h e-mail-enumtypes.c

libemail_engine_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(EVOLUTION_DATA_SERVER_CFLAGS) \
	$(GNOME_PLATFORM_CFLAGS) \
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
	$(NULL)

libmailengineincludedir = $(privincludedir)/libemail-engine
libmailengineinclude_HEADERS =  \
	camel-null-store.h \
	e-mail-enums.h \
	e-mail-enumtypes.h \
	e-mail-folder-utils.h \
	e-mail-junk-filter.h \
	e-mail-session-utils.h \
	e-mail-session.h \
	e-mail-store-utils.h \
	e-mail-utils.h \
	mail-config.h \
	mail-folder-cache.h \
	mail-ops.h \
	mail-tools.h \
	mail-vfolder.h	\
	$(NULL)

libemail_engine_la_SOURCES =  \
	$(libmailengineinclude_HEADERS) \
	camel-null-store.c \
	e-mail-enumtypes.c \
	e-mail-folder-utils.c \
	e-mail-junk-filter.c \
	e-mail-session-utils.c \
	e-mail-session.c \
	e-mail-store-utils.c \
	e-mail-utils.c \
	mail-config.c \
	mail-folder-cache.c \
	mail-ops.c \
	mail-tools.c \
	mail-vfolder.c	\
	$(NULL)

libemail_engine_la_LIBADD = \
	$(top_builddir)/libemail-utils/libemail-utils.la \
	$(EVOLUTION_DATA_SERVER_LIBS) \
	$(GNOME_PLATFORM_LIBS) \
	$(NULL)

libemail_engine_la_LDFLAGS = -avoid-version $(NO_UNDEFINED)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libemail-engine.pc

BUILT_SOURCES = $(ENUM_GENERATED)

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(pkgconfig_DATA)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
