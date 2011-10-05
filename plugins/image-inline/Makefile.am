@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-image-inline.eplug

plugin_LTLIBRARIES = liborg-gnome-image-inline.la

liborg_gnome_image_inline_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(GTKIMAGEVIEW_CFLAGS)

liborg_gnome_image_inline_la_SOURCES = image-inline.c

liborg_gnome_image_inline_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_image_inline_la_LIBADD =			\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la	\
	$(GTKIMAGEVIEW_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

EXTRA_DIST = org-gnome-image-inline.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
