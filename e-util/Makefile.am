SUBDIRS = . ename

imagesdir = $(datadir)/images/evolution

INCLUDES =					\
	-I$(top_srcdir)				\
	$(GNOME_INCLUDEDIR)			\
	$(UNICODE_CFLAGS)			\
	-DEVOLUTION_IMAGES=\""$(imagesdir)"\"	\
	-DG_LOG_DOMAIN=\"e-utils\"		\
	$(UNICODE_CFLAGS)

noinst_LTLIBRARIES = libeutil.la libeutil-static.la

libeutil_la_SOURCES =			\
	e-dialog-widgets.c		\
	e-dialog-widgets.h		\
	e-gtk-utils.c			\
	e-gtk-utils.h			\
	e-gui-utils.c			\
	e-gui-utils.h			\
	e-html-utils.c			\
	e-html-utils.h			\
	e-iterator.c			\
	e-iterator.h			\
	e-list-iterator.c		\
	e-list-iterator.h		\
	e-list.c			\
	e-list.h			\
	e-sexp.c			\
	e-sexp.h

libeutil_la_LIBADD = $(UNICODE_LIBS)

libeutil_static_la_SOURCES = $(libeutil_la_SOURCES)
libeutil_static_la_LDFLAGS = --all-static
