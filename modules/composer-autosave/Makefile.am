module_LTLIBRARIES = module-composer-autosave.la

module_composer_autosave_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-composer-autosave\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)

module_composer_autosave_la_SOURCES = \
	evolution-composer-autosave.c				\
	e-autosave-utils.c					\
	e-autosave-utils.h					\
	e-composer-autosave.c					\
	e-composer-registry.c

module_composer_autosave_la_LIBADD = \
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/composer/libcomposer.la			\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)

module_composer_autosave_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
