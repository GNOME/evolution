## CORBA stuff

IDLS =												   	\
	$(top_srcdir)/composer/Evolution-Composer.idl						  	\
	$(top_srcdir)/calendar/idl/evolution-calendar.idl						\
	$(top_srcdir)/addressbook/gui/component/select-names/Evolution-Addressbook-SelectNames.idl

# Message composer IDL files

COMPOSER_IDL_GENERATED_H = \
        Evolution-Composer.h
COMPOSER_IDL_GENERATED_C =             			\
        Evolution-Composer-common.c       		\
        Evolution-Composer-skels.c        		\
        Evolution-Composer-stubs.c
COMPOSER_IDL_GENERATED = $(COMPOSER_IDL_GENERATED_C) $(COMPOSER_IDL_GENERATED_H)

$(COMPOSER_IDL_GENERATED_H): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES)   \
                $(srcdir)/../../composer/Evolution-Composer.idl
$(COMPOSER_IDL_GENERATED_C): $(COMPOSER_IDL_GENERATED_H)


# SelectNames IDL files

SELECT_NAMES_IDL_GENERATED_H =              		\
	Evolution-Addressbook-SelectNames.h
SELECT_NAMES_IDL_GENERATED_C =                 		\
	Evolution-Addressbook-SelectNames-common.c	\
	Evolution-Addressbook-SelectNames-skels.c	\
	Evolution-Addressbook-SelectNames-stubs.c
SELECT_NAMES_IDL_GENERATED = $(SELECT_NAMES_IDL_GENERATED_C) $(SELECT_NAMES_IDL_GENERATED_H)

$(SELECT_NAMES_IDL_GENERATED_H): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES)   \
                $(top_srcdir)/addressbook/gui/component/select-names/Evolution-Addressbook-SelectNames.idl
$(SELECT_NAMES_IDL_GENERATED_C): $(SELECT_NAMES_IDL_GENERATED_H)


IDL_GENERATED = $(COMPOSER_IDL_GENERATED) $(SELECT_NAMES_IDL_GENERATED)


SUBDIRS = alarm-notify dialogs

component_LTLIBRARIES = libevolution-calendar.la

INCLUDES =						\
	-DG_LOG_DOMAIN=\"calendar-gui\"			\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/calendar/cal-client		\
	-I$(top_builddir)/calendar/cal-client		\
	-I$(top_srcdir)/libical/src			\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/a11y/calendar			\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	$(EVOLUTION_CALENDAR_CFLAGS)

glade_DATA = 					\
	e-itip-control.glade			\
	goto-dialog.glade

etspec_DATA =				\
	e-calendar-table.etspec		\
	e-meeting-time-sel.etspec	\
	e-cal-list-view.etspec

libevolution_calendar_la_SOURCES =		\
	$(IDL_GENERATED)			\
	cal-search-bar.c			\
	cal-search-bar.h			\
	calendar-config.c			\
	calendar-config.h			\
	calendar-commands.c			\
	calendar-commands.h			\
	calendar-component.c			\
	calendar-component.h			\
	calendar-offline-handler.c		\
	calendar-offline-handler.h		\
	calendar-view.c				\
	calendar-view.h				\
	calendar-view-factory.c			\
	calendar-view-factory.h			\
	comp-editor-factory.c			\
	comp-editor-factory.h			\
	comp-util.c				\
	comp-util.h				\
	control-factory.c			\
	control-factory.h			\
	e-alarm-list.c				\
	e-alarm-list.h				\
	e-cal-model-calendar.c			\
	e-cal-model-calendar.h			\
	e-cal-model-tasks.c			\
	e-cal-model-tasks.h			\
	e-cal-model.c				\
	e-cal-model.h				\
	e-cal-view.c				\
	e-cal-view.h				\
	e-cal-list-view.c			\
	e-cal-list-view.h			\
	e-calendar-table.h			\
	e-calendar-table.c			\
	e-cell-date-edit-text.h			\
	e-cell-date-edit-text.c			\
	e-comp-editor-registry.c		\
	e-comp-editor-registry.h		\
	e-date-time-list.c                      \
	e-date-time-list.h                      \
	e-day-view-layout.c			\
	e-day-view-layout.h			\
	e-day-view-main-item.c			\
	e-day-view-main-item.h			\
	e-day-view-time-item.c			\
	e-day-view-time-item.h			\
	e-day-view-top-item.c			\
	e-day-view-top-item.h			\
	e-day-view.c				\
	e-day-view.h				\
	e-itip-control.h			\
	e-itip-control.c			\
	e-meeting-attendee.c			\
	e-meeting-attendee.h			\
	e-meeting-list-view.c			\
	e-meeting-list-view.h			\
	e-meeting-store.c			\
	e-meeting-store.h			\
	e-meeting-time-sel.c			\
	e-meeting-time-sel.h			\
	e-meeting-time-sel-item.c		\
	e-meeting-time-sel-item.h		\
	e-meeting-types.h			\
	e-meeting-utils.c			\
	e-meeting-utils.h			\
	e-select-names-editable.c		\
	e-select-names-editable.h		\
	e-select-names-renderer.c		\
	e-select-names-renderer.h		\
	e-week-view-event-item.c		\
	e-week-view-event-item.h		\
	e-week-view-layout.c			\
	e-week-view-layout.h			\
	e-week-view-main-item.c			\
	e-week-view-main-item.h			\
	e-week-view-titles-item.c		\
	e-week-view-titles-item.h		\
	e-week-view.c				\
	e-week-view.h				\
	e-tasks.c				\
	e-tasks.h				\
	e-timezone-entry.c			\
	e-timezone-entry.h			\
	gnome-cal.c				\
	gnome-cal.h				\
	goto.c					\
	goto.h					\
	itip-bonobo-control.c			\
	itip-bonobo-control.h			\
	itip-utils.c				\
	itip-utils.h				\
	main.c					\
	misc.c					\
	misc.h					\
	print.c					\
	print.h					\
	tag-calendar.c				\
	tag-calendar.h				\
	tasks-control.c				\
	tasks-control.h				\
	weekday-picker.c			\
	weekday-picker.h

libevolution_calendar_la_LIBADD =					\
	$(DB3_LDADD)							\
	$(top_builddir)/widgets/menus/libmenus.la			\
	$(top_builddir)/shell/libeshell.la				\
	$(top_builddir)/calendar/cal-client/libcal-client.la		\
	$(top_builddir)/addressbook/backend/ebook/libebook.la		\
	$(top_builddir)/calendar/gui/dialogs/libcal-dialogs.la		\
	$(top_builddir)/widgets/e-timezone-dialog/libetimezonedialog.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/a11y/calendar/libevolution-calendar-a11y.la                       \
	$(EVOLUTION_CALENDAR_LIBS)

libevolution_calendar_la_LDFLAGS = -avoid-version -module

server_in_files = GNOME_Evolution_Calendar.server.in.in

server_DATA = $(server_in_files:.server.in.in=.server)
%.server.in: %.server.in.in
	sed -e "s|\@COMPONENTDIR\@|$(componentdir)|" $< > $@

# GConf schemas

schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = apps_evolution_calendar.schemas

@INTLTOOL_SERVER_RULE@

EXTRA_DIST =	 			\
	$(glade_DATA)			\
	$(schema_DATA)                  \
	$(etspec_DATA)			\
	$(server_in_files)

BUILT_SOURCES = $(IDL_GENERATED) $(server_DATA)
CLEANFILES = $(BUILT_SOURCES)

install-data-local:
	$(mkinstalldirs) $(Conduitsdir); \
	if test -z "$(DESTDIR)"; then \
		for p in $(schema_DATA); do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$$p; \
		done; \
	fi

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
