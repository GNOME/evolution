module_LTLIBRARIES = libevolution-module-test.la

libevolution_module_test_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/shell			\
	-I$(top_srcdir)/widgets			\
	-I$(top_srcdir)/widgets/menus		\
	-I$(top_srcdir)/widgets/misc		\
	-I$(top_builddir)/shell			\
	-DG_LOG_DOMAIN=\"evolution-test\"	\
	$(GNOME_PLATFORM_CFLAGS)

libevolution_module_test_la_SOURCES =		\
	evolution-module-test.c			\
	e-test-shell-backend.c			\
	e-test-shell-backend.h			\
	e-test-shell-view.c			\
	e-test-shell-view.h

libevolution_module_test_la_LIBADD =		\
	$(top_builddir)/shell/libeshell.la	\
	$(GNOME_PLATFORM_LIBS)

libevolution_module_test_la_LDFLAGS =		\
	-avoid-version -module $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
