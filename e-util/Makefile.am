eutilincludedir = $(privincludedir)/e-util
ecpsdir = $(privdatadir)/ecps
ruledir = $(privdatadir)

include $(top_srcdir)/glib-gen.mak
glib_enum_headers=e-util-enums.h
glib_enum_output=e-util-enumtypes
glib_enum_define=E
glib_enum_prefix=e

ENUM_GENERATED = e-util-enumtypes.h e-util-enumtypes.c
MARSHAL_GENERATED = e-marshal.c e-marshal.h

if OS_WIN32
PLATFORM_SOURCES = e-win32-reloc.c e-win32-defaults.c e-win32-defaults.h
endif

privsolib_LTLIBRARIES = libeutil.la

eutilinclude_HEADERS = 				\
	e-activity.h				\
	e-alert.h				\
	e-alert-dialog.h			\
	e-alert-sink.h				\
	e-bit-array.h				\
	e-categories-config.h			\
	e-charset.h				\
	e-config.h				\
	e-datetime-format.h			\
	e-dialog-utils.h			\
	e-dialog-widgets.h			\
	e-event.h				\
	e-file-utils.h				\
	e-html-utils.h				\
	e-icon-factory.h			\
	e-import.h				\
	e-marshal.h				\
	e-mktemp.h				\
	e-poolv.h				\
	e-print.h				\
	e-plugin.h				\
	e-plugin-ui.h				\
	e-plugin-util.h				\
	e-selection.h				\
	e-sorter.h				\
	e-sorter-array.h			\
	e-text-event-processor-emacs-like.h	\
	e-text-event-processor-types.h		\
	e-text-event-processor.h		\
	e-ui-manager.h				\
	e-util.h				\
	e-util-enums.h				\
	e-util-enumtypes.h			\
	e-unicode.h				\
	e-xml-utils.h

libeutil_la_CPPFLAGS =							\
	$(AM_CPPFLAGS)							\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_srcdir)/widgets						\
	-DEVOLUTION_BINDIR=\""$(bindir)"\"				\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"				\
	-DEVOLUTION_ECPSDIR=\""$(ecpsdir)"\"				\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"			\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"			\
	-DEVOLUTION_HELPDIR=\""$(evolutionhelpdir)"\"			\
	-DEVOLUTION_ICONDIR=\""$(icondir)"\"				\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"			\
	-DEVOLUTION_LIBDIR=\""$(datadir)"\"				\
	-DEVOLUTION_LIBEXECDIR=\""$(privlibexecdir)"\"			\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"			\
	-DEVOLUTION_MODULEDIR=\""$(moduledir)"\"		\
	-DEVOLUTION_PLUGINDIR=\""$(plugindir)"\"			\
	-DEVOLUTION_PREFIX=\""$(prefix)"\"				\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"			\
	-DEVOLUTION_SOUNDDIR=\""$(soundsdir)"\"				\
	-DEVOLUTION_SYSCONFDIR=\""$(sysconfdir)"\"			\
	-DEVOLUTION_TOOLSDIR=\""$(privlibexecdir)"\"			\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"				\
	-DEVOLUTION_RULEDIR=\"$(ruledir)\"				\
	-DG_LOG_DOMAIN=\"e-utils\"					\
	$(EVOLUTION_DATA_SERVER_CFLAGS)					\
	$(GNOME_PLATFORM_CFLAGS)

libeutil_la_SOURCES =				\
	$(eutilinclude_HEADERS)			\
	e-activity.c				\
	e-alert.c				\
	e-alert-dialog.c			\
	e-alert-sink.c				\
	e-bit-array.c				\
	e-categories-config.c			\
	e-charset.c				\
	e-config.c				\
	e-datetime-format.c			\
	e-dialog-utils.c			\
	e-dialog-widgets.c			\
	e-event.c				\
	e-file-utils.c				\
	e-html-utils.c				\
	e-icon-factory.c			\
	e-import.c				\
	e-marshal.c				\
	e-mktemp.c				\
	e-poolv.c				\
	e-plugin.c				\
	e-plugin-ui.c				\
	e-plugin-util.c				\
	e-print.c				\
	e-selection.c				\
	e-sorter.c				\
	e-sorter-array.c			\
	e-text-event-processor-emacs-like.c	\
	e-text-event-processor.c		\
	e-ui-manager.c				\
	e-util.c				\
	e-unicode.c				\
	e-util-enumtypes.c			\
	e-util-private.h			\
	e-xml-utils.c				\
	$(PLATFORM_SOURCES)

libeutil_la_LDFLAGS = $(NO_UNDEFINED)

libeutil_la_LIBADD = 			\
	$(top_builddir)/libemail-utils/libemail-utils.la		\
	$(ICONV_LIBS)			\
	$(EVOLUTION_DATA_SERVER_LIBS)	\
	$(GNOME_PLATFORM_LIBS)		\
	$(INTLLIBS)

error_DATA = e-system.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

EXTRA_DIST =				\
	e-system.error.xml		\
	e-marshal.list

BUILT_SOURCES = $(ENUM_GENERATED) $(MARSHAL_GENERATED) $(error_DATA)
CLEANFILES    = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
