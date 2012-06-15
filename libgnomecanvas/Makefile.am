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
	gnome-canvas-i18n.h			\
	gnome-canvas-pixbuf.h			\
	gnome-canvas-rect.h			\
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
	gnome-canvas-pixbuf.c			\
	gnome-canvas-rect.c			\
	gnome-canvas-text.c			\
	gnome-canvas-util.c			\
	gnome-canvas-widget.c			\
	gnome-canvas.c

include $(top_srcdir)/glib-gen.mak
glib_enum_prefix=gnome_canvas

MARSHAL_GENERATED =				\
	gnome-canvas-marshal.c			\
	gnome-canvas-marshal.h

libgnomecanvas_la_LIBADD = \
	$(GNOME_PLATFORM_LIBS) \
	$(MATH_LIB)

libgnomecanvas_la_LDFLAGS = -avoid-version $(NO_UNDEFINED)

BUILT_SOURCES = $(MARSHAL_GENERATED)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	gnome-canvas-marshal.list

-include $(top_srcdir)/git.mk
