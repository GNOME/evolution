bin_PROGRAMS = evolution-calendar-importer

sounddir = $(datadir)/evolution/sounds

INCLUDES = 						\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_SOUNDDIR=\""$(sounddir)"\"		\
	-DG_LOG_DOMAIN=\"Evolution-Importer\"		\
	-I$(top_srcdir)					\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/calendar/cal-client		\
	-I$(top_builddir)/calendar/cal-client		\
	-I$(top_srcdir)/calendar			\
	-I$(top_builddir)/calendar			\
	-I$(top_srcdir)/libical/src/libical		\
	-I$(top_builddir)/libical/src/libical		\
	-I$(top_srcdir)/libical/src/libicalvcal		\
	$(EVOLUTION_CALENDAR_CFLAGS)

evolution_calendar_importer_SOURCES = 	\
	evolution-calendar-importer.h	\
	icalendar-importer.c		\
	main.c

evolution_calendar_importer_LDADD =				\
	$(top_builddir)/shell/importer/libevolution-importer.la \
	$(top_builddir)/calendar/cal-util/libcal-util.la	\
	$(top_builddir)/calendar/cal-client/libcal-client.la	\
	$(top_builddir)/libwombat/libwombat.la			\
	$(top_builddir)/libical/src/libical/libical-evolution.la	\
	$(top_builddir)/libical/src/libicalvcal/libicalvcal-evolution.la \
	$(EVOLUTION_CALENDAR_LIBS)

oafdir = $(datadir)/oaf
oaf_in_files = GNOME_Evolution_Calendar_Importer.oaf.in

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA)

@XML_I18N_MERGE_OAF_RULE@
