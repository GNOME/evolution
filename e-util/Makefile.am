eutilincludedir = $(privincludedir)/e-util
econdincludedir = $(privincludedir)/e-conduit
ecpsdir = $(privdatadir)/ecps
ruledir = $(privdatadir)

if OS_WIN32
WIN32_BOOTSTRAP_LIBS = $(top_builddir)/win32/libemiscwidgets.la
PLATFORM_SOURCES = e-win32-reloc.c
endif

AM_CPPFLAGS =								\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_srcdir)/widgets						\
	-DEVOLUTION_CATEGORY_ICONS=\""$(imagesdir)/categories"\"	\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"				\
	-DEVOLUTION_ECPSDIR=\""$(ecpsdir)"\"				\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"			\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"				\
	-DEVOLUTION_HELPDIR=\""$(evolutionhelpdir)"\"			\
	-DEVOLUTION_ICONSDIR=\""$(imagesdir)"\"				\
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
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"			\
	-DSEARCH_RULE_DIR=\"$(ruledir)\"				\
	-DG_LOG_DOMAIN=\"e-utils\"					\
	$(GNOME_PILOT_CFLAGS)						\
	$(E_UTIL_CFLAGS)

privsolib_LTLIBRARIES = libeutil.la libeconduit.la

eutilinclude_HEADERS = 				\
	e-bconf-map.h				\
	e-binding.h				\
	e-categories-config.h			\
	e-config.h				\
	e-corba-utils.h				\
	e-cursor.h				\
	e-datetime-format.h			\
	e-dialog-utils.h			\
	e-dialog-widgets.h			\
	e-error.h				\
	e-event.h				\
	e-folder-map.h				\
	e-fsutils.h				\
	e-html-utils.h				\
	e-icon-factory.h			\
	e-import.h				\
	e-logger.h				\
	e-non-intrusive-error-dialog.h		\
	e-marshal.h				\
	e-menu.h				\
	e-mktemp.h				\
	e-print.h				\
	e-plugin.h				\
	e-plugin-ui.h				\
	e-popup.h				\
	e-profile-event.h			\
	e-request.h				\
	e-signature.h				\
	e-signature-list.h			\
	e-bit-array.h				\
	e-sorter.h				\
	e-sorter-array.h			\
	e-text-event-processor-emacs-like.h	\
	e-text-event-processor-types.h		\
	e-text-event-processor.h		\
	e-util.h				\
	e-util-labels.h				\
	e-xml-utils.h

libeutil_la_SOURCES =				\
	$(eutilinclude_HEADERS)			\
	e-bconf-map.c				\
	e-binding.c				\
	e-categories-config.c			\
	e-config.c				\
	e-corba-utils.c				\
	e-cursor.c				\
	e-datetime-format.c			\
	e-dialog-utils.c			\
	e-dialog-widgets.c			\
	e-error.c				\
	e-event.c				\
	e-folder-map.c				\
	e-fsutils.c				\
	e-html-utils.c				\
	e-icon-factory.c			\
	e-import.c				\
	e-logger.c				\
	e-non-intrusive-error-dialog.c		\
	e-marshal.c				\
	e-menu.c				\
	e-mktemp.c				\
	e-plugin.c				\
	e-plugin-ui.c				\
	e-popup.c				\
	e-print.c				\
	e-profile-event.c			\
	e-request.c				\
	e-signature.c				\
	e-signature-list.c			\
	e-bit-array.c				\
	e-sorter.c				\
	e-sorter-array.c			\
	e-text-event-processor-emacs-like.c     \
	e-text-event-processor.c                \
	e-util.c				\
	e-util-labels.c				\
	e-util-private.h			\
	e-xml-utils.c				\
	gconf-bridge.c				\
	gconf-bridge.h				\
	$(PLATFORM_SOURCES)

MARSHAL_GENERATED = e-marshal.c e-marshal.h
@EVO_MARSHAL_RULE@

libeutil_la_LDFLAGS = $(NO_UNDEFINED)

libeutil_la_LIBADD = 		\
	$(WIN32_BOOTSTRAP_LIBS)	\
	$(ICONV_LIBS)		\
	$(E_UTIL_LIBS)		\
	$(GNOME_PILOT_LIBS)	\
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
