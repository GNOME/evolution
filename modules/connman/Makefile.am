module_LTLIBRARIES = libevolution-module-connman.la

libevolution_module_connman_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-DG_LOG_DOMAIN=\"evolution-connman\"		\
	$(GNOME_PLATFORM_CFLAGS)

libevolution_module_connman_la_SOURCES =		\
	evolution-connman.c

libevolution_module_connman_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/shell/libeshell.la		\
	$(GNOME_PLATFORM_LIBS)

libevolution_module_connman_la_LDFLAGS =		\
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
