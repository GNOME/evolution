module_LTLIBRARIES = libevolution-module-plugin-manager.la

libevolution_module_plugin_manager_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/widgets					\
	-DG_LOG_DOMAIN=\"evolution-plugin-manager\"		\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(SHELL_CFLAGS)

libevolution_module_plugin_manager_la_SOURCES =			\
	evolution-plugin-manager.c

libevolution_module_plugin_manager_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(GNOME_PLATFORM_LIBS)					\
	$(SHELL_LIBS)

libevolution_module_plugin_manager_la_LDFLAGS =			\
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
