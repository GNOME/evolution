module_LTLIBRARIES = module-contact-photos.la

module_contact_photos_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-contact-photos\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)					\
	$(NULL)

module_contact_photos_la_SOURCES = \
	evolution-contact-photos.c				\
	e-contact-photo-source.c				\
	e-contact-photo-source.h				\
	e-photo-cache-contact-loader.c				\
	e-photo-cache-contact-loader.h				\
	$(NULL)

module_contact_photos_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)						\
	$(NULL)

module_contact_photos_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
