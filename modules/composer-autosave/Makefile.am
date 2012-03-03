module_LTLIBRARIES = libevolution-module-composer-autosave.la

libevolution_module_composer_autosave_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/widgets					\
	-DG_LOG_DOMAIN=\"evolution-composer-autosave\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)

libevolution_module_composer_autosave_la_SOURCES =		\
	evolution-composer-autosave.c				\
	e-autosave-utils.c					\
	e-autosave-utils.h					\
	e-composer-autosave.c					\
	e-composer-registry.c

libevolution_module_composer_autosave_la_LIBADD =		\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/composer/libcomposer.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/libevolution-utils/libevolution-utils.la \
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)

libevolution_module_composer_autosave_la_LDFLAGS =		\
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
