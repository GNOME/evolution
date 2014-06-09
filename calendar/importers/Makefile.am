privsolib_LTLIBRARIES = libevolution-calendar-importers.la

libevolution_calendar_importers_la_CPPFLAGS = 		\
	$(AM_CPPFLAGS)					\
	-DEVOLUTION_SOUNDDIR=\""$(soundsdir)"\"		\
	-DG_LOG_DOMAIN=\"Evolution-Importer\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_builddir)/calendar			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

libevolution_calendar_importers_la_SOURCES = \
	evolution-calendar-importer.h	     \
	icalendar-importer.c

libevolution_calendar_importers_la_LDFLAGS = -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

libevolution_calendar_importers_la_LIBADD =			\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/shell/libevolution-shell.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(NULL)

-include $(top_srcdir)/git.mk
