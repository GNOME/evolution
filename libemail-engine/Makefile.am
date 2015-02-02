NULL =

privsolib_LTLIBRARIES = libemail-engine.la

ENUM_TYPES = e-mail-engine-enums.h

e-mail-engine-enumtypes.h: $(top_srcdir)/enumtypes.h.template $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(top_srcdir)/enumtypes.h.template \
		--fhead "#ifndef E_MAIL_ENGINE_ENUMTYPES_H\n#define E_MAIL_ENGINE_ENUMTYPES_H\n" \
		--ftail "#endif /* E_MAIL_ENGINE_ENUMTYPES_H */\n" \
		$(addprefix $(srcdir)/,$(ENUM_TYPES)) > $@

e-mail-engine-enumtypes.c: $(top_srcdir)/enumtypes.c.template $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(top_srcdir)/enumtypes.c.template \
		--fhead "#include \"e-mail-engine-enumtypes.h\"" \
		$(addprefix $(srcdir)/,$(ENUM_TYPES)) > $@

ENUM_GENERATED = e-mail-engine-enumtypes.h e-mail-engine-enumtypes.c

libemail_engine_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-DLIBEMAIL_ENGINE_COMPILATION \
	$(EVOLUTION_DATA_SERVER_CFLAGS) \
	$(GNOME_PLATFORM_CFLAGS) \
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\" \
	$(CODE_COVERAGE_CFLAGS) \
	$(NULL)

libmailengineincludedir = $(privincludedir)/libemail-engine
libmailengineinclude_HEADERS =  \
	libemail-engine.h \
	camel-null-store.h \
	camel-sasl-xoauth2.h \
	e-mail-engine-enums.h \
	e-mail-engine-enumtypes.h \
	e-mail-folder-utils.h \
	e-mail-junk-filter.h \
	e-mail-session-utils.h \
	e-mail-session.h \
	e-mail-store-utils.h \
	e-mail-utils.h \
	em-filter-folder-element.h \
	em-vfolder-context.h \
	em-vfolder-rule.h \
	mail-config.h \
	mail-folder-cache.h \
	mail-mt.h \
	mail-ops.h \
	mail-tools.h \
	mail-vfolder.h \
	$(NULL)

libemail_engine_la_SOURCES =  \
	$(libmailengineinclude_HEADERS) \
	camel-null-store.c \
	camel-sasl-xoauth2.c \
	e-mail-engine-enumtypes.c \
	e-mail-folder-utils.c \
	e-mail-junk-filter.c \
	e-mail-session-utils.c \
	e-mail-session.c \
	e-mail-store-utils.c \
	e-mail-utils.c \
	em-filter-folder-element.c \
	em-vfolder-context.c \
	em-vfolder-rule.c \
	mail-config.c \
	mail-folder-cache.c \
	mail-mt.c \
	mail-ops.c \
	mail-tools.c \
	mail-vfolder.c \
	$(NULL)

libemail_engine_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la \
	$(EVOLUTION_DATA_SERVER_LIBS) \
	$(GNOME_PLATFORM_LIBS) \
	$(NULL)

libemail_engine_la_LDFLAGS = -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libemail-engine.pc

BUILT_SOURCES = $(ENUM_GENERATED)

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(pkgconfig_DATA)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
