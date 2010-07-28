module_LTLIBRARIES = libevolution-module-network-manager.la

libevolution_module_network_manager_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-network-manager\"		\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(NM_CFLAGS)

libevolution_module_network_manager_la_SOURCES =		\
	evolution-network-manager.c

libevolution_module_network_manager_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(GNOME_PLATFORM_LIBS)

libevolution_module_network_manager_la_LDFLAGS =		\
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
