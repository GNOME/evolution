module_LTLIBRARIES = libevolution-module-windows-sens.la

libevolution_module_windows_sens_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-windows-sens\"		\
	$(GNOME_PLATFORM_CFLAGS)

libevolution_module_windows_sens_la_SOURCES =		\
	evolution-windows-sens.c

libevolution_module_windows_sens_la_LIBADD =			\
	$(top_builddir)/shell/libeshell.la			\
	$(GNOME_PLATFORM_LIBS)					\
	$(SENS_LIBS)

libevolution_module_windows_sens_la_LDFLAGS =		\
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
