## CORBA stuff

IDLS =												   	\
	$(top_srcdir)/composer/Evolution-Composer.idl						  	\
	$(top_srcdir)/calendar/idl/evolution-calendar.idl						\
	$(top_srcdir)/addressbook/gui/component/select-names/Evolution-Addressbook-SelectNames.idl

CALENDAR_IDL_GENERATED_H =              		\
	evolution-calendar.h
CALENDAR_IDL_GENERATED_C =		\
	evolution-calendar-common.c	\
	evolution-calendar-skels.c	\
	evolution-calendar-stubs.c
CALENDAR_IDL_GENERATED = $(CALENDAR_IDL_GENERATED_C) $(CALENDAR_IDL_GENERATED_H)

$(CALENDAR_IDL_GENERATED_H): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES)   \
                $(top_srcdir)/calendar/idl/evolution-calendar.idl
$(CALENDAR_IDL_GENERATED_C): $(CALENDAR_IDL_GENERATED_H)

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


IDL_GENERATED = $(CALENDAR_IDL_GENERATED) $(COMPOSER_IDL_GENERATED) $(SELECT_NAMES_IDL_GENERATED)

# The marshallers
MARSHAL_GENERATED = e-calendar-marshal.c e-calendar-marshal.h
@EVO_MARSHAL_RULE@

SUBDIRS = alarm-notify dialogs

component_LTLIBRARIES = libevolution-calendar.la

ecalendarincludedir = $(privincludedir)/calendar/gui

ecalendarinclude_HEADERS =	\
	e-cal-config.h

INCLUDES =						\
	-DG_LOG_DOMAIN=\"calendar-gui\"			\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_srcdir)/a11y/calendar			\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	$(LIBSOUP_CFLAGS)				\
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
	$(MARSHAL_GENERATED)			\
	cal-search-bar.c			\
	cal-search-bar.h			\
	calendar-config.c			\
	calendar-config.h			\
	calendar-config-keys.h			\
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
	e-cal-component-preview.c		\
	e-cal-component-preview.h		\
	e-cal-config.c				\
	e-cal-config.h				\
	e-cal-menu.c				\
	e-cal-menu.h				\
	e-cal-model-calendar.h			\
	e-cal-model-calendar.c			\
	e-cal-model-calendar.h			\
	e-cal-model-tasks.c			\
	e-cal-model-tasks.h			\
	e-cal-model.c				\
	e-cal-model.h				\
	e-cal-popup.h				\
	e-cal-popup.c				\
	e-calendar-view.c			\
	e-calendar-view.h			\
	e-cal-list-view.c			\
	e-cal-list-view.h			\
	e-cal-list-view-config.c		\
	e-cal-list-view-config.h		\
	e-calendar-table.c			\
	e-calendar-table.h			\
	e-calendar-table-config.c		\
	e-calendar-table-config.h		\
	e-cell-date-edit-config.c		\
	e-cell-date-edit-config.h		\
	e-cell-date-edit-text.h			\
	e-cell-date-edit-text.c			\
	e-comp-editor-registry.c		\
	e-comp-editor-registry.h		\
	e-date-edit-config.c			\
	e-date-edit-config.h			\
	e-date-time-list.c                      \
	e-date-time-list.h                      \
	e-day-view-config.c			\
	e-day-view-config.h			\
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
	e-mini-calendar-config.c		\
	e-mini-calendar-config.h		\
	e-pub-utils.c				\
	e-pub-utils.h				\
	e-select-names-editable.c		\
	e-select-names-editable.h		\
	e-select-names-renderer.c		\
	e-select-names-renderer.h		\
	e-week-view-config.c			\
	e-week-view-config.h			\
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
	migration.c				\
	migration.h				\
	misc.c					\
	misc.h					\
	print.c					\
	print.h					\
	tag-calendar.c				\
	tag-calendar.h				\
	tasks-component.c			\
	tasks-component.h			\
	tasks-control.c				\
	tasks-control.h				\
	weekday-picker.c			\
	weekday-picker.h

libevolution_calendar_la_LIBADD =					\
	$(top_builddir)/widgets/menus/libmenus.la			\
	$(top_builddir)/shell/libeshell.la				\
	$(top_builddir)/calendar/common/libevolution-calendarprivate.la	\
	$(top_builddir)/calendar/gui/dialogs/libcal-dialogs.la		\
	$(top_builddir)/widgets/e-timezone-dialog/libetimezonedialog.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/a11y/calendar/libevolution-calendar-a11y.la	\
	$(LIBSOUP_LIBS)							\
	$(EVOLUTION_CALENDAR_LIBS)

libevolution_calendar_la_LDFLAGS = -avoid-version -module

server_in_files = GNOME_Evolution_Calendar.server.in.in
server_DATA = $(server_in_files:.server.in.in=_$(BASE_VERSION).server)
@EVO_SERVER_RULE@
@INTLTOOL_SERVER_RULE@

# GConf schemas

schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps_evolution_calendar.schemas.in.in
schema_DATA = $(schema_in_files:.schemas.in.in=-$(BASE_VERSION).schemas)
%-$(BASE_VERSION).schemas.in: %.schemas.in.in
	cp $< $@

@INTLTOOL_SCHEMAS_RULE@

EXTRA_DIST =	 			\
	e-calendar-marshal.list		\
	$(glade_DATA)			\
	$(schema_in_files)              \
	$(etspec_DATA)			\
	$(server_in_files)

BUILT_SOURCES = $(IDL_GENERATED) $(server_DATA)
CLEANFILES = $(BUILT_SOURCES)

install-data-local:
	if test -z "$(DESTDIR)"; then \
		for p in $(schema_DATA); do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$$p; \
		done; \
	fi

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
