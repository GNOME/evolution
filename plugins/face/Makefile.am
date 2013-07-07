@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-face.eplug

plugin_LTLIBRARIES = liborg-gnome-face.la

liborg_gnome_face_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-I. 						\
	-I$(top_srcdir)					\
	-I$(top_builddir)/mail				\
	-I$(top_builddir)/composer			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

liborg_gnome_face_la_SOURCES = face.c

liborg_gnome_face_la_LIBADD = 					\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/composer/libevolution-mail-composer.la	\
	$(top_builddir)/mail/libevolution-mail.la 		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(NULL)

liborg_gnome_face_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

error_DATA = org-gnome-face.error

errordir = $(privdatadir)/errors

EXTRA_DIST = 				\
	org-gnome-face.eplug.xml	\
	org-gnome-face.error.xml

BUILT_SOURCES = 			\
	org-gnome-face.eplug 		\
	org-gnome-face.error

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
