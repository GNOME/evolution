component_LTLIBRARIES = libevolution-test.la

AM_CPPFLAGS =					\
	-I$(top_srcdir)/shell			\
	-I$(top_srcdir)/widgets/misc		\
	-I$(top_builddir)/shell			\
	-DG_LOG_DOMAIN=\"evolution-test\"	\
	$(EVOLUTION_TEST_CFLAGS)

libevolution_test_la_SOURCES =			\
	evolution-test-component.c		\
	evolution-test-component.h

libevolution_test_la_LIBADD =			\
	$(top_builddir)/shell/libeshell.la	\
	$(EVOLUTION_TEST_LIBS)

libevolution_test_la_LDFLAGS =			\
	-avoid-version -module $(NO_UNDEFINED)

testserver_in_files = GNOME_Evolution_Test.server.in.in
testserver_DATA = $(testserver_in_files:.server.in.in=.server)
testserverdir = $(serverdir)
@EVO_SERVER_RULE@
@INTLTOOL_SERVER_RULE@

BUILT_SOURCES = $(testserver_DATA)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST =					\
	$(testserver_in_files)			\
	GNOME_Evolution_Test.server.in.in

dist-hook:
	cd $(distdir); rm -f $(BUILD_SOURCES)

-include $(top_srcdir)/git.mk
