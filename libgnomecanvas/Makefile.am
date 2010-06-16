libgnomecanvasincludedir = $(privincludedir)/libgnomecanvas

privsolib_LTLIBRARIES = libgnomecanvas.la

libgnomecanvas_la_CPPFLAGS = \
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir) 				\
	$(GNOME_PLATFORM_CFLAGS)			\
	-DGNOMECANVASLIBDIR=\""$(libdir)"\" 		\
	-DGNOMECANVASDATADIR=\""$(datadir)"\" 		\
	-DGNOMECANVASPIXMAPDIR=\""$(datadir)/pixmaps"\"	\
	-DGNOMECANVASBINDIR=\""$(bindir)"\" 		\
	-DGNOMECANVASLOCALSTATEDIR=\""$(localstatedir)"\" 	\
	-DG_LOG_DOMAIN=\"GnomeCanvas\"

libgnomecanvasinclude_HEADERS =			\
	gailcanvas.h				\
	gailcanvasgroup.h			\
	gailcanvasgroupfactory.h		\
	gailcanvasitem.h			\
	gailcanvasitemfactory.h			\
	gailcanvastext.h			\
	gailcanvastextfactory.h			\
	gailcanvaswidget.h			\
	gailcanvaswidgetfactory.h		\
	gnome-canvas-bpath.h			\
	gnome-canvas-clipgroup.h		\
	gnome-canvas-i18n.h			\
	gnome-canvas-line.h			\
	gnome-canvas-path-def.h			\
	gnome-canvas-pixbuf.h			\
	gnome-canvas-polygon.h			\
	gnome-canvas-rect-ellipse.h		\
	gnome-canvas-rich-text.h		\
	gnome-canvas-shape.h			\
	gnome-canvas-text.h			\
	gnome-canvas-util.h			\
	gnome-canvas-widget.h			\
	gnome-canvas.h				\
	libgnomecanvas.h

libgnomecanvas_la_SOURCES =			\
	gailcanvas.c				\
	gailcanvasgroup.c			\
	gailcanvasgroupfactory.c		\
	gailcanvasitem.c			\
	gailcanvasitemfactory.c			\
	gailcanvastext.c			\
	gailcanvastextfactory.c			\
	gailcanvaswidget.c			\
	gailcanvaswidgetfactory.c		\
	gnome-canvas-bpath.c			\
	gnome-canvas-clipgroup.c		\
	gnome-canvas-line.c			\
	gnome-canvas-path-def.c			\
	gnome-canvas-pixbuf.c			\
	gnome-canvas-polygon.c			\
	gnome-canvas-rect-ellipse.c		\
	gnome-canvas-rich-text.c		\
	gnome-canvas-shape-private.h		\
	gnome-canvas-shape.c			\
	gnome-canvas-text.c			\
	gnome-canvas-util.c			\
	gnome-canvas-widget.c			\
	gnome-canvas.c				\
	libgnomecanvastypes.c

MARSHAL_GENERATED =				\
	gnome-canvas-marshal.c			\
	gnome-canvas-marshal.h

@EVO_MARSHAL_RULE@

libgnomecanvas_la_LIBADD = \
	$(top_builddir)/libart_lgpl/libart_lgpl.la	\
	$(GNOME_PLATFORM_LIBS)

libgnomecanvas_la_LDFLAGS = $(NO_UNDEFINED)

BUILT_SOURCES = $(MARSHAL_GENERATED)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	gnome-canvas-marshal.list

-include $(top_srcdir)/git.mk
