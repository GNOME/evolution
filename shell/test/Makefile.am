module_LTLIBRARIES = libevolution-test.la

INCLUDES =					\
	-I$(top_srcdir)/shell			\
	-I$(top_srcdir)/widgets/misc		\
	-I$(top_builddir)/shell			\
	-DG_LOG_DOMAIN=\"evolution-test\"	\
	$(EVOLUTION_TEST_CFLAGS)

libevolution_test_la_SOURCES =			\
	e-test-shell-module.c			\
	e-test-shell-view.c			\
	e-test-shell-view.h

libevolution_test_la_LIBADD =			\
	$(top_builddir)/shell/libeshell.la	\
	$(EVOLUTION_TEST_LIBS)

libevolution_test_la_LDFLAGS =			\
	-avoid-version -module $(NO_UNDEFINED)
