INCLUDES =							\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/shell					\
	-I$(top_builddir)/shell					\
	-I$(top_srcdir)/calendar				\
	-I$(top_builddir)/calendar/cal-client			\
	-I$(top_srcdir)/libical/src/libical			\
	-I$(top_builddir)/libical/src/libical			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"			\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"			\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"		\
	-DEVOLUTION_BUTTONSDIR=\""$(buttonsdir)"\"		\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"		\
	-DLOCATIONDIR=\""$(Locationdir)"\"			\
	-DPREFIX=\""$(prefix)"\"	\
	-DG_LOG_DOMAIN=\"evolution-executive-summary\"		\
	$(EVOLUTION_EXECUTIVE_SUMMARY_CFLAGS)

IDLS =								\
	$(top_srcdir)/mail/Mailer.idl

IDL_GENERATED_H =						\
	Mailer.h
IDL_GENERATED_C =						\
	Mailer-common.c						\
	Mailer-skels.c						\
	Mailer-stubs.c
IDL_GENERATED = $(IDL_GENERATED_C) $(IDL_GENERATED_H)

$(IDL_GENERATED_H): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) $(top_srcdir)/mail/Mailer.idl
$(IDL_GENERATED_C): $(IDL_GENERATED_H)

summary_sources =				\
	$(IDL_GENERATED)			\
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

summary_libs =				 				\
	$(top_builddir)/shell/libeshell.la				\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/calendar/cal-client/libcal-client.la		\
	$(top_builddir)/calendar/cal-util/libcal-util.la		\
	$(EVOLUTION_EXECUTIVE_SUMMARY_LIBS)

component_LTLIBRARIES = libevolution-executive-summary.la
libevolution_executive_summary_la_SOURCES = $(summary_sources)
libevolution_executive_summary_la_LIBADD = $(summary_libs)
libevolution_executive_summary_la_LDFLAGS = -module -avoid-version 

Locationdir = $(privdatadir)
Location_DATA = Locations

# .server files

server_in_files = GNOME_Evolution_Summary.server.in.in

server_DATA = $(server_in_files:.server.in.in=.server)
%.server.in: %.server.in.in
	sed -e "s|\@COMPONENTDIR\@|$(componentdir)|" $< > $@

@INTLTOOL_SERVER_RULE@


# Glade

glade_DATA = my-evolution.glade

# GConf schemas

schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = apps_evolution_summary.schemas

install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$$p; \
		done \
	fi

# Extra

EXTRA_DIST = 				\
	$(schema_DATA)			\
	$(summary_sources)		\
	$(server_in_files)		\
	$(server_DATA)			\
	$(Location_DATA)		\
	$(glade_DATA)			\
	ChangeLog.pre-1-4		\
	Locations.h			\
	Location-translation-script	\
	check-empty.xpm			\
	check-filled.xpm		\
	check-none.xpm

BUILT_SOURCES = $(IDL_GENERATED) $(server_DATA)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
