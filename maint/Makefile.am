### Process this file with automake to produce Makefile.in

gladecatalog_DATA = evolution.xml
gladecatalogdir = `$(PKG_CONFIG) --variable=catalogdir gladeui-2.0`

gladeevolution_LTLIBRARIES = libgladeevolution.la
gladeevolutiondir = `$(PKG_CONFIG) --variable=moduledir gladeui-2.0`

libgladeevolution_la_SOURCES =

libgladeevolution_la_LDFLAGS = \
	-module -avoid-version

libgladeevolution_la_LIBADD = \
	$(top_srcdir)/e-util/libevolution-util.la \
	$(top_srcdir)/mail/libevolution-mail.la \
	$(top_srcdir)/calendar/gui/libevolution-calendar.la \
	$(EVOLUTION_DATA_SERVER_LIBS) \
	$(GNOME_PLATFORM_LIBS)

EXTRA_DIST = \
	$(gladecatalog_DATA)

-include $(top_srcdir)/git.mk
