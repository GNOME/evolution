module_LTLIBRARIES = module-gravatar.la

module_gravatar_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-gravatar\"			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)					\
	$(NULL)

module_gravatar_la_SOURCES = \
	evolution-module-gravatar.c				\
	e-gravatar-photo-source.c				\
	e-gravatar-photo-source.h				\
	e-photo-cache-gravatar-loader.c				\
	e-photo-cache-gravatar-loader.h				\
	$(NULL)

module_gravatar_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)						\
	$(NULL)

module_gravatar_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
