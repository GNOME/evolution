INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_srcdir)/shell					\
	-I$(top_builddir)/shell					\
	-I$(top_srcdir)/calendar				\
	-I$(top_builddir)/calendar/cal-client			\
	-I$(top_srcdir)/libical/src/libical			\
	-I$(top_builddir)/libical/src/libical			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"			\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"            	\
	-DG_LOG_DOMAIN=\"evolution-executive-summary\"		\
	$(EVOLUTION_EXECUTIVE_SUMMARY_CFLAGS)

IDLS =								\
	$(top_srcdir)/mail/Mailer.idl

MAIL_GENERATED =						\
	Mailer.h						\
	Mailer-common.c						\
	Mailer-skels.c						\
	Mailer-stubs.c

$(MAIL_GENERATED): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) `$(GNOME_CONFIG) --cflags idl` $(top_srcdir)/mail/Mailer.idl

summary_sources =				\
	$(MAIL_GENERATED)			\
	component-factory.c			\
	component-factory.h			\
	e-cell-tri.c				\
	e-cell-tri.h				\
	e-summary.c				\
	e-summary.h				\
	e-summary-calendar.c			\
	e-summary-calendar.h			\
	e-summary-factory.c			\
	e-summary-factory.h			\
	e-summary-mail.c			\
	e-summary-mail.h			\
	e-summary-offline-handler.c		\
	e-summary-offline-handler.h		\
	e-summary-preferences.c			\
	e-summary-preferences.h			\
	e-summary-rdf.c				\
	e-summary-rdf.h				\
	e-summary-shown.c			\
	e-summary-shown.h			\
	e-summary-table.c			\
	e-summary-table.h			\
	e-summary-tasks.c			\
	e-summary-tasks.h			\
	e-summary-type.h			\
	e-summary-weather.c			\
	e-summary-weather.h			\
	metar.c					\
	metar.h					\
	my-evolution-html.h			\
	weather.h

summary_exe_sources = \
	main.c

summary_libs =				 				\
	$(top_builddir)/calendar/gui/alarm-notify/libalarm.a		\
	$(top_builddir)/shell/libeshell.la				\
	$(top_builddir)/widgets/misc/libemiscwidgets.a			\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/calendar/cal-client/libcal-client.la		\
	$(top_builddir)/calendar/cal-util/libcal-util.la		\
	$(top_builddir)/libversit/libversit.a				\
	$(top_builddir)/libical/src/libical/libical-evolution.la	\
	$(top_builddir)/libwombat/libwombat.la				\
	$(EVOLUTION_EXECUTIVE_SUMMARY_LIBS)

if ENABLE_SHLIB_COMPONENTS

lib_LTLIBRARIES = libevolution-executive-summary.la

libevolution_executive_summary_la_SOURCES = \
	$(summary_sources)

libevolution_executive_summary_la_LIBADD = \
	$(summary_libs)

else

bin_PROGRAMS = evolution-executive-summary

evolution_executive_summary_SOURCES =		\
	$(summary_sources)			\
	$(summary_exe_sources)

evolution_executive_summary_LDADD =		\
	$(summary_libs)

endif

Locationdir = $(datadir)/evolution
Location_DATA = Locations

oaf_files = GNOME_Evolution_Summary.oaf
oaf_in_files = $(oaf_files:.oaf=.oaf.in)
oaf_in_in_files  = $(oaf_files:.oaf=.oaf.in.in)

oafdir = $(datadir)/oaf
oaf_DATA = $(oaf_files)

gladedir = $(datadir)/evolution/glade
glade_DATA = my-evolution.glade

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST = 				\
	$(summary_sources)		\
	$(summary_exe_sources)		\
	$(oaf_files)			\
	$(oaf_in_files)			\
	$(oaf_in_in_files)		\
	$(oaf_DATA)			\
	$(Location_DATA)		\
	$(glade_DATA)			\
	Locations.h			\
	Location-translation-script	\
	check-empty.xpm			\
	check-filled.xpm		\
	check-none.xpm

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
