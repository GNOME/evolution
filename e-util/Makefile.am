SUBDIRS = . ename

imagesdir = $(datadir)/images/evolution

eutilincludedir = $(includedir)/evolution/e-util
econdincludedir = $(includedir)/evolution/e-conduit
edb3includedir = $(includedir)/evolution/e-db3util

INCLUDES =								\
	-I$(top_srcdir)							\
	-DEVOLUTION_IMAGES=\""$(imagesdir)"\"				\
	-DEVOLUTION_CATEGORY_ICONS=\""$(imagesdir)/categories"\"	\
	-DG_LOG_DOMAIN=\"e-utils\"					\
	$(DB3_CFLAGS)							\
	$(PISOCK_CFLAGS)						\
	$(E_UTIL_CFLAGS)

privlib_LTLIBRARIES = libeutil.la libedb3util.la libeconduit.la
noinst_LTLIBRARIES = libeutil-static.la libeconduit-static.la

eutilinclude_HEADERS = 				\
	e-bonobo-factory-util.h			\
	e-categories-master-list-wombat.h	\
	e-categories-config.h			\
	e-component-listener.h			\
	e-config-listener.h			\
	e-corba-utils.h				\
	e-dialog-utils.h			\
	e-dialog-widgets.h			\
	e-gtk-utils.h				\
	e-gui-utils.h				\
	e-host-utils.h				\
	e-html-utils.h				\
	e-iterator.h				\
	e-lang-utils.h				\
	e-list-iterator.h			\
	e-list.h				\
	e-memory.h				\
	e-mktemp.h				\
	e-msgport.h				\
	e-passwords.h				\
	e-path.h				\
	e-proxy.h				\
	e-request.h				\
	e-sexp.h				\
	e-time-utils.h				\
	e-url.h					\
	e-xml-hash-utils.h			\
	md5-utils.h

libeutil_la_SOURCES =				\
	$(eutilinclude_HEADERS)			\
	e-bonobo-factory-util.c			\
	e-categories-master-list-wombat.c	\
	e-categories-config.c			\
	e-component-listener.c			\
	e-config-listener.c			\
	e-corba-utils.c				\
	e-dialog-utils.c			\
	e-dialog-widgets.c			\
	e-gtk-utils.c				\
	e-gui-utils.c				\
	e-host-utils.c				\
	e-html-utils.c				\
	e-iterator.c				\
	e-lang-utils.c				\
	e-list-iterator.c			\
	e-list.c				\
	e-memory.c				\
	e-mktemp.c				\
	e-msgport.c				\
	e-passwords.c				\
	e-path.c				\
	e-proxy.c				\
	e-request.c				\
	e-sexp.c				\
	e-time-utils.c				\
	e-url.c					\
	e-xml-hash-utils.c			\
	md5-utils.c

libeutil_la_LIBADD = $(E_UTIL_LIBS)

libeutil_static_la_SOURCES = $(libeutil_la_SOURCES)
libeutil_static_la_LIBADD  = $(libeutil_la_LIBADD)
libeutil_static_la_LDFLAGS = -all-static

econdinclude_HEADERS =		\
	e-pilot-map.h		\
	e-pilot-settings.h	\
	e-pilot-util.h

pilot_sources = \
	$(econdinclude_HEADERS)	\
	e-pilot-map.c		\
	e-pilot-settings.c	\
	e-pilot-util.c

if ENABLE_PILOT_CONDUITS
pilot_compile = $(pilot_sources)
else
pilot_compile = e-pilot-settings.c e-pilot-settings.h
endif

libeconduit_la_SOURCES = $(pilot_compile)

libeconduit_static_la_SOURCES = $(libeconduit_la_SOURCES)
libeconduit_static_la_LDFLAGS = -all-static

EXTRA_DIST = $(pilot_sources)

edb3include_HEADERS =			\
	e-db3-utils.h			\
	e-dbhash.h

libedb3util_la_SOURCES =		\
	$(edb3include_HEADERS)		\
	e-db3-utils.c			\
	e-dbhash.c
