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
	e-canvas-utils.c		\
	e-canvas-utils.h		\
	e-canvas-vbox.c			\
	e-canvas-vbox.h			\
	e-canvas.c			\
	e-canvas.h			\
	e-cursors.c			\
	e-cursors.h			\
	e-dialog-widgets.c		\
	e-dialog-widgets.h		\
	e-font.c			\
	e-font.h			\
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
	e-popup-menu.c			\
	e-popup-menu.h			\
	e-printable.c			\
	e-printable.h			\
	e-sexp.c			\
	e-sexp.h			\
	e-unicode.c			\
	e-unicode.h			\
	e-util.c			\
	e-util.h			\
	e-xml-utils.c			\
	e-xml-utils.h

libeutil_la_LIBADD = $(UNICODE_LIBS)

libeutil_static_la_SOURCES = $(libeutil_la_SOURCES)
libeutil_static_la_LDFLAGS = --all-static