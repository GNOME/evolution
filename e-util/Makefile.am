SUBDIRS = . ename

imagesdir = $(datadir)/images/evolution

INCLUDES =					\
	$(DB3_CFLAGS)				\
	-I$(top_srcdir)				\
	$(GNOME_INCLUDEDIR)			\
	$(EXTRA_GNOME_CFLAGS)			\
	$(PISOCK_CFLAGS)			\
	-DEVOLUTION_IMAGES=\""$(imagesdir)"\"	\
	-DG_LOG_DOMAIN=\"e-utils\"

noinst_LTLIBRARIES = libeutil.la libeutil-static.la \
	libeconduit.la libeconduit-static.la

libeutil_la_SOURCES =			\
	e-corba-utils.c			\
	e-corba-utils.h			\
	e-dialog-widgets.c		\
	e-dialog-widgets.h		\
	e-gtk-utils.c			\
	e-gtk-utils.h			\
	e-gui-utils.c			\
	e-gui-utils.h			\
	e-host-utils.c			\
	e-host-utils.h			\
	e-html-utils.c			\
	e-html-utils.h			\
	e-iterator.c			\
	e-iterator.h			\
	e-list-iterator.c		\
	e-list-iterator.h		\
	e-list.c			\
	e-list.h			\
	e-memory.c			\
	e-memory.h			\
	e-msgport.c			\
	e-msgport.h			\
	e-path.c			\
	e-path.h			\
	e-sexp.c			\
	e-sexp.h			\
	e-time-utils.c			\
	e-time-utils.h			\
	e-url.c				\
	e-url.h				\
	e-db3-utils.c			\
	e-db3-utils.h			\
	e-dbhash.c			\
	e-dbhash.h			\
	md5-utils.c			\
	md5-utils.h

libeutil_la_LIBADD = $(GAL_LIBS)

libeutil_static_la_SOURCES = $(libeutil_la_SOURCES)
libeutil_static_la_LIBADD  = $(libeutil_la_LIBADD)
libeutil_static_la_LDFLAGS = --all-static

pilot_sources = \
	e-pilot-map.c	\
	e-pilot-map.h	\
	e-pilot-util.c	\
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

