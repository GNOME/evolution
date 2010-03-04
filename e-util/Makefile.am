eutilincludedir = $(privincludedir)/e-util
econdincludedir = $(privincludedir)/e-conduit
ecpsdir = $(privdatadir)/ecps
ruledir = $(privdatadir)

if OS_WIN32
PLATFORM_SOURCES = e-win32-reloc.c
endif

privsolib_LTLIBRARIES = libeutil.la libeconduit.la

eutilinclude_HEADERS = 				\
	e-account-utils.h			\
	e-activity.h				\
	e-alert.h				\
	e-alert-activity.h			\
	e-alert-dialog.h			\
	e-bconf-map.h				\
	e-binding.h				\
	e-bit-array.h				\
	e-categories-config.h			\
	e-charset.h				\
	e-config.h				\
	e-datetime-format.h			\
	e-dialog-utils.h			\
	e-dialog-widgets.h			\
	e-event.h				\
	e-file-utils.h				\
	e-folder-map.h				\
	e-html-utils.h				\
	e-icon-factory.h			\
	e-import.h				\
	e-io-activity.h				\
	e-logger.h				\
	e-marshal.h				\
	e-mktemp.h				\
	e-module.h				\
	e-non-intrusive-error-dialog.h		\
	e-print.h				\
	e-plugin.h				\
	e-plugin-ui.h				\
	e-plugin-util.h				\
	e-profile-event.h			\
	e-selection.h				\
	e-signature.h				\
	e-signature-list.h			\
	e-signature-utils.h			\
	e-sorter.h				\
	e-sorter-array.h			\
	e-text-event-processor-emacs-like.h	\
	e-text-event-processor-types.h		\
	e-text-event-processor.h		\
	e-timeout-activity.h			\
	e-ui-manager.h				\
	e-util.h				\
	e-unicode.h				\
	e-xml-utils.h				\
	gconf-bridge.h

libeutil_la_CPPFLAGS =							\
	$(AM_CPPFLAGS)							\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_srcdir)/widgets						\
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
	$(GNOME_PLATFORM_CFLAGS)					\
	$(GNOME_PILOT_CFLAGS)						\
	$(E_UTIL_CFLAGS)

libeutil_la_SOURCES =				\
	$(eutilinclude_HEADERS)			\
	e-account-utils.c			\
	e-activity.c				\
	e-alert.c				\
	e-alert-activity.c			\
	e-alert-dialog.c			\
	e-bconf-map.c				\
	e-binding.c				\
	e-bit-array.c				\
	e-categories-config.c			\
	e-charset.c				\
	e-config.c				\
	e-datetime-format.c			\
	e-dialog-utils.c			\
	e-dialog-widgets.c			\
	e-event.c				\
	e-file-utils.c				\
	e-folder-map.c				\
	e-html-utils.c				\
	e-icon-factory.c			\
	e-import.c				\
	e-io-activity.c				\
	e-logger.c				\
	e-marshal.c				\
	e-mktemp.c				\
	e-module.c				\
	e-non-intrusive-error-dialog.c		\
	e-plugin.c				\
	e-plugin-ui.c				\
	e-plugin-util.c				\
	e-print.c				\
	e-profile-event.c			\
	e-selection.c				\
	e-signature.c				\
	e-signature-list.c			\
	e-signature-utils.c			\
	e-sorter.c				\
	e-sorter-array.c			\
	e-text-event-processor-emacs-like.c	\
	e-text-event-processor.c		\
	e-timeout-activity.c			\
	e-ui-manager.c				\
	e-util.c				\
	e-unicode.c				\
	e-util-private.h			\
	e-xml-utils.c				\
	gconf-bridge.c				\
	$(PLATFORM_SOURCES)

MARSHAL_GENERATED = e-marshal.c e-marshal.h
@EVO_MARSHAL_RULE@

libeutil_la_LDFLAGS = $(NO_UNDEFINED)

libeutil_la_LIBADD = 		\
	$(ICONV_LIBS)		\
	$(E_UTIL_LIBS)		\
	$(GNOME_PILOT_LIBS)	\
	$(GNOME_PLATFORM_LIBS)	\
	$(INTLLIBS)

econdinclude_HEADERS =		\
	e-pilot-map.h		\
	e-pilot-util.h

pilot_sources = \
	$(econdinclude_HEADERS)	\
	e-pilot-map.c		\
	e-pilot-util.c

if ENABLE_PILOT_CONDUITS
pilot_compile = $(pilot_sources)
else
pilot_compile = e-html-utils.c
endif

libeconduit_la_CPPFLAGS =							\
	$(AM_CPPFLAGS)							\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_srcdir)/widgets						\
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
	-DEVOLUTION_PLUGINDIR=\""$(plugindir)"\"			\
	-DEVOLUTION_PREFIX=\""$(prefix)"\"				\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"			\
	-DEVOLUTION_SOUNDDIR=\""$(soundsdir)"\"				\
	-DEVOLUTION_SYSCONFDIR=\""$(sysconfdir)"\"			\
	-DEVOLUTION_TOOLSDIR=\""$(privlibexecdir)"\"			\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"				\
	-DEVOLUTION_RULEDIR=\"$(ruledir)\"				\
	-DG_LOG_DOMAIN=\"e-utils\"					\
	$(GNOME_PILOT_CFLAGS)						\
	$(E_UTIL_CFLAGS)

libeconduit_la_SOURCES = $(pilot_compile)

libeconduit_la_LDFLAGS = $(NO_UNDEFINED)

libeconduit_la_LIBADD = $(E_UTIL_LIBS) 	$(GNOME_PILOT_LIBS)

error_DATA = e-system.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

EXTRA_DIST =				\
	e-system.error.xml		\
	$(pilot_sources)		\
	ChangeLog.pre-1-4		\
	e-marshal.list

BUILT_SOURCES = $(MARSHAL_GENERATED) $(error_DATA)
CLEANFILES    = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
