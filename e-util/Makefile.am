SUBDIRS = . ename

imagesdir = $(datadir)/images/evolution

INCLUDES =								\
	-I$(top_srcdir)							\
	-DEVOLUTION_IMAGES=\""$(imagesdir)"\"				\
	-DEVOLUTION_CATEGORY_ICONS=\""$(imagesdir)/categories"\"	\
	-DG_LOG_DOMAIN=\"e-utils\"					\
	$(DB3_CFLAGS)							\
	$(PISOCK_CFLAGS)						\
	$(E_UTIL_CFLAGS)

noinst_LTLIBRARIES = libeutil.la libeutil-static.la \
	libeconduit.la libeconduit-static.la \
	libedb3util.la

libeutil_la_SOURCES =				\
	e-bonobo-factory-util.c			\
	e-bonobo-factory-util.h			\
	e-categories-master-list-wombat.c	\
	e-categories-master-list-wombat.h	\
	e-categories-config.c			\
	e-categories-config.h			\
	e-corba-utils.c				\
	e-corba-utils.h				\
	e-dialog-utils.c			\
	e-dialog-utils.h			\
	e-dialog-widgets.c			\
	e-dialog-widgets.h			\
	e-gtk-utils.c				\
	e-gtk-utils.h				\
	e-gui-utils.c				\
	e-gui-utils.h				\
	e-host-utils.c				\
	e-host-utils.h				\
	e-html-utils.c				\
	e-html-utils.h				\
	e-iterator.c				\
	e-iterator.h				\
	e-lang-utils.c				\
	e-lang-utils.h				\
	e-list-iterator.c			\
	e-list-iterator.h			\
	e-list.c				\
	e-list.h				\
	e-memory.c				\
	e-memory.h				\
	e-mktemp.c				\
	e-mktemp.h				\
	e-msgport.c				\
	e-msgport.h				\
	e-passwords.c				\
	e-passwords.h				\
	e-path.c				\
	e-path.h				\
	e-request.c				\
	e-request.h				\
	e-sexp.c				\
	e-sexp.h				\
	e-time-utils.c				\
	e-time-utils.h				\
	e-url.c					\
	e-url.h					\
	md5-utils.c				\
	md5-utils.h

libeutil_la_LIBADD = $(E_UTIL_LIBS)

libeutil_static_la_SOURCES = $(libeutil_la_SOURCES)
libeutil_static_la_LIBADD  = $(libeutil_la_LIBADD)
libeutil_static_la_LDFLAGS = --all-static

pilot_sources = \
	e-pilot-map.c		\
	e-pilot-map.h		\
	e-pilot-settings.c	\
	e-pilot-settings.h	\
	e-pilot-util.c		\
	e-pilot-util.h

if ENABLE_PILOT_CONDUITS
pilot_compile = $(pilot_sources)
else
pilot_compile = 
endif

libeconduit_la_SOURCES = $(pilot_compile)

libeconduit_static_la_SOURCES = $(libeconduit_la_SOURCES)
libeconduit_static_la_LDFLAGS = --all-static

EXTRA_DIST = $(pilot_sources)

libedb3util_la_SOURCES =		\
	e-db3-utils.c			\
	e-db3-utils.h			\
	e-dbhash.c			\
	e-dbhash.h
