privsolib_LTLIBRARIES = libgnomecanvas-2.la

libgnomecanvas_2_la_CPPFLAGS = \
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir) 				\
	$(GNOME_PLATFORM_CFLAGS)			\
	-DGNOMECANVASLIBDIR=\""$(libdir)"\" 		\
	-DGNOMECANVASDATADIR=\""$(datadir)"\" 		\
	-DGNOMECANVASPIXMAPDIR=\""$(datadir)/pixmaps"\"	\
	-DGNOMECANVASBINDIR=\""$(bindir)"\" 		\
	-DGNOMECANVASLOCALSTATEDIR=\""$(localstatedir)"\" 	\
	-DG_LOG_DOMAIN=\"GnomeCanvas\"

libgnomecanvas_2_la_LIBADD = \
	$(top_builddir)/libart_lgpl/libart_lgpl_2.la	\
	$(GNOME_PLATFORM_LIBS)

libgnomecanvas_2_la_LDFLAGS = $(NO_UNDEFINED)

libgnomecanvas_2_la_SOURCES =			\
	gailcanvas.c				\
	gailcanvas.h				\
	gailcanvasgroup.c			\
	gailcanvasgroup.h			\
	gailcanvasgroupfactory.c		\
	gailcanvasgroupfactory.h		\
	gailcanvasitem.c			\
	gailcanvasitem.h			\
	gailcanvasitemfactory.c			\
	gailcanvasitemfactory.h			\
	gailcanvastext.c			\
	gailcanvastext.h			\
	gailcanvastextfactory.c			\
	gailcanvastextfactory.h			\
	gailcanvaswidget.c			\
	gailcanvaswidget.h			\
	gailcanvaswidgetfactory.c		\
	gailcanvaswidgetfactory.h		\
	gnome-canvas-bpath.c			\
	gnome-canvas-bpath.h			\
	gnome-canvas-clipgroup.c		\
	gnome-canvas-clipgroup.h		\
	gnome-canvas-i18n.h			\
	gnome-canvas-line.c			\
	gnome-canvas-line.h			\
	gnome-canvas-path-def.c			\
	gnome-canvas-path-def.h			\
	gnome-canvas-pixbuf.c			\
	gnome-canvas-pixbuf.h			\
	gnome-canvas-polygon.c			\
	gnome-canvas-polygon.h			\
	gnome-canvas-rect-ellipse.c		\
	gnome-canvas-rect-ellipse.h		\
	gnome-canvas-rich-text.c		\
	gnome-canvas-rich-text.h		\
	gnome-canvas-shape-private.h		\
	gnome-canvas-shape.c			\
	gnome-canvas-shape.h			\
	gnome-canvas-text.c			\
	gnome-canvas-text.h			\
	gnome-canvas-util.c			\
	gnome-canvas-util.h			\
	gnome-canvas-widget.c			\
	gnome-canvas-widget.h			\
	gnome-canvas.c				\
	gnome-canvas.h				\
	libgnomecanvas.h			\
	libgnomecanvastypes.c

MARSHAL_GENERATED =				\
	gnome-canvas-marshal.c			\
	gnome-canvas-marshal.h

@EVO_MARSHAL_RULE@

BUILT_SOURCES = $(MARSHAL_GENERATED)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	gnome-canvas-marshal.list

-include $(top_srcdir)/git.mk
