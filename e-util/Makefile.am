eutilincludedir = $(privincludedir)/e-util
econdincludedir = $(privincludedir)/e-conduit

if ENABLE_MONO
MONOHEADERS = e-plugin-mono.h
MONOSOURCES = e-plugin-mono.c
endif

INCLUDES =								\
	-I$(top_srcdir)							\
	-DEVOLUTION_IMAGES=\""$(imagesdir)"\"				\
	-DEVOLUTION_ICONSDIR=\""$(imagesdir)"\"				\
	-DEVOLUTION_CATEGORY_ICONS=\""$(imagesdir)/categories"\"	\
	-DEVOLUTION_PLUGINDIR=\""$(plugindir)"\"			\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"			\
	-DG_LOG_DOMAIN=\"e-utils\"					\
	$(GNOME_PILOT_CFLAGS)						\
	$(E_UTIL_CFLAGS)

privlib_LTLIBRARIES = libeutil.la libeconduit.la

eutilinclude_HEADERS = 				\
	e-account-list.h			\
	e-account.h				\
	e-bconf-map.h				\
	e-categories-config.h			\
	e-categories-master-list-wombat.h	\
	e-component-listener.h			\
	e-config.h				\
	e-config-listener.h			\
	e-corba-utils.h				\
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
	$(MONOHEADERS)

libeutil_la_SOURCES =				\
	$(MARSHAL_GENERATED)			\
	$(eutilinclude_HEADERS)			\
	e-account-list.c			\
	e-account.c				\
	e-bconf-map.c				\
	e-categories-config.c			\
	e-categories-master-list-wombat.c	\
	e-component-listener.c			\
	e-config.c				\
	e-config-listener.c			\
	e-corba-utils.c				\
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
	$(MONOSOURCES)

MARSHAL_GENERATED = e-util-marshal.c e-util-marshal.h
@EVO_MARSHAL_RULE@

libeutil_la_LIBADD = $(E_UTIL_LIBS)

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
