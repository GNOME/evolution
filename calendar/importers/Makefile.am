importersdir = $(privlibdir)/evolution-calendar-importers

importers_LTLIBRARIES = libevolution-calendar-importers.la

# privlibexec_PROGRAMS = evolution-calendar-importer

INCLUDES = 						\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_SOUNDDIR=\""$(soundsdir)"\"		\
	-DG_LOG_DOMAIN=\"Evolution-Importer\"		\
	-I$(top_srcdir)					\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/calendar			\
	-I$(top_builddir)/calendar			\
	$(EVOLUTION_CALENDAR_CFLAGS)

libevolution_calendar_importers_la_SOURCES = \
	evolution-calendar-importer.h	     \
	icalendar-importer.c		     \
	main.c

libevolution_calendar_importers_la_LDFLAGS = -avoid-version -module

libevolution_calendar_importers_la_LIBADD = \
	$(top_builddir)/shell/importer/libevolution-importer.la 	\
	$(top_builddir)/calendar/common/libevolution-calendarprivate.la	\
	$(IMPORTERS_LIBS)

# evolution_calendar_importer_SOURCES = 	\
# 	evolution-calendar-importer.h	\
# 	icalendar-importer.c		\
# 	main.c
#
# evolution_calendar_importer_LDADD =				\
# 	$(top_builddir)/shell/libeshell.la			\
# 	$(top_builddir)/shell/importer/libevolution-importer.la \
# 	$(top_builddir)/calendar/cal-util/libcal-util.la	\
# 	$(top_builddir)/calendar/cal-client/libcal-client.la	\
# 	$(top_builddir)/libwombat/libwombat.la			\
# 	$(EVOLUTION_CALENDAR_LIBS)

server_in_files = GNOME_Evolution_Calendar_Importer.server.in.in
server_DATA = $(server_in_files:.server.in.in=_$(BASE_VERSION).server)
@EVO_SERVER_RULE@
@INTLTOOL_SERVER_RULE@

BUILT_SOURCES = $(server_DATA)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = $(server_in_files)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
