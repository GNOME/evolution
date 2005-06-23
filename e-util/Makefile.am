eutilincludedir = $(privincludedir)/e-util
econdincludedir = $(privincludedir)/e-conduit

if OS_WIN32
WIN32_BOOTSTRAP_LIBS = $(top_builddir)/win32/libemiscwidgets.la
PLATFORM_SOURCES = e-win32-reloc.c
endif

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_srcdir)/widgets						\
	-DEVOLUTION_PREFIX=\""$(prefix)"\"				\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"				\
	-DEVOLUTION_HELPDIR=\""$(evolutionhelpdir)"\"			\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"			\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"			\
	-DEVOLUTION_ICONSDIR=\""$(imagesdir)"\"				\
	-DEVOLUTION_CATEGORY_ICONS=\""$(imagesdir)/categories"\"	\
	-DEVOLUTION_PLUGINDIR=\""$(plugindir)"\"			\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"			\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"			\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"			\
	-DSEARCH_RULE_DIR=\"$(ruledir)\"				\
	-DG_LOG_DOMAIN=\"e-utils\"					\
	$(GNOME_PILOT_CFLAGS)						\
	$(EXTRA_GNOME_CFLAGS)                   			\
	$(GNOME_FULL_CFLAGS)						\
	$(ICONV_CFLAGS)                        				\
	$(E_UTIL_CFLAGS)

privlib_LTLIBRARIES = libeutil.la libeconduit.la

eutilinclude_HEADERS = 				\
	e-account-list.h			\
	e-account.h				\
	e-bconf-map.h				\
	e-categories-config.h			\
	e-component-listener.h			\
	e-config.h				\
	e-config-listener.h			\
	e-corba-utils.h				\
	e-cursor.h				\
	e-dialog-utils.h			\
	e-dialog-widgets.h			\
	e-error.h				\
	e-event.h				\
	e-folder-map.h				\
	e-fsutils.h				\
	e-gtk-utils.h				\
	e-gui-utils.h				\
	e-html-utils.h				\
	e-icon-factory.h			\
	e-import.h				\
	e-iterator.h				\
	e-list-iterator.h			\
	e-list.h				\
	e-menu.h				\
	e-mktemp.h				\
	e-print.h				\
	e-plugin.h				\
	e-popup.h				\
	e-profile-event.h			\
	e-request.h				\
	e-signature.h				\
	e-signature-list.h			\
	e-time-utils.h				\
	e-uid.h					\
	md5-utils.h				\
	e-bit-array.h				\
	e-iconv.h				\
	e-sorter.h				\
	e-sorter-array.h			\
	e-text-event-processor-emacs-like.h	\
	e-text-event-processor-types.h		\
	e-text-event-processor.h		\
	e-util.h				\
	e-util-marshal.h			\
	e-xml-utils.h

libeutil_la_SOURCES =				\
	$(eutilinclude_HEADERS)			\
	e-util-marshal.c			\
	e-account-list.c			\
	e-account.c				\
	e-bconf-map.c				\
	e-categories-config.c			\
	e-component-listener.c			\
	e-config.c				\
	e-config-listener.c			\
	e-corba-utils.c				\
	e-cursor.c				\
	e-dialog-utils.c			\
	e-dialog-widgets.c			\
	e-error.c				\
	e-event.c				\
	e-folder-map.c				\
	e-fsutils.c				\
	e-gtk-utils.c				\
	e-gui-utils.c				\
	e-html-utils.c				\
	e-icon-factory.c			\
	e-import.c				\
	e-iterator.c				\
	e-list-iterator.c			\
	e-list.c				\
	e-menu.c				\
	e-mktemp.c				\
	e-plugin.c				\
	e-popup.c				\
	e-print.c				\
	e-profile-event.c			\
	e-request.c				\
	e-signature.c				\
	e-signature-list.c			\
	e-time-utils.c				\
	e-uid.c					\
	eggtrayicon.c				\
	eggtrayicon.h				\
	md5-utils.c				\
	e-bit-array.c				\
	e-i18n.h				\
	e-iconv.c				\
	e-sorter.c				\
	e-sorter-array.c			\
	e-text-event-processor-emacs-like.c     \
	e-text-event-processor.c                \
	e-util.c				\
	e-util-private.h			\
	e-xml-utils.c				\
	$(PLATFORM_SOURCES)

MARSHAL_GENERATED = e-util-marshal.c e-util-marshal.h
@EVO_MARSHAL_RULE@

libeutil_la_LDFLAGS = $(NO_UNDEFINED)

libeutil_la_LIBADD = 		\
	$(WIN32_BOOTSTRAP_LIBS)	\
	$(ICONV_LIBS)		\
	$(E_UTIL_LIBS)

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
pilot_compile = md5-utils.c
endif

libeconduit_la_SOURCES = $(pilot_compile)

libeconduit_la_LDFLAGS = $(NO_UNDEFINED)

libeconduit_la_LIBADD = $(E_UTIL_LIBS)

error_DATA = e-system.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

EXTRA_DIST =				\
	e-system.error.xml		\
	$(pilot_sources)		\
	ChangeLog.pre-1-4		\
	e-util-marshal.list

BUILT_SOURCES = $(MARSHAL_GENERATED) $(error_DATA)
CLEANFILES    = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
