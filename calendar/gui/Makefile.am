SUBDIRS = alarm-notify dialogs

privsolib_LTLIBRARIES = libevolution-calendar.la

ecalendarincludedir = $(privincludedir)/calendar/gui

ecalendarinclude_HEADERS =			\
	cal-editor-utils.h			\
	calendar-config.h			\
	calendar-config-keys.h			\
	calendar-view.h				\
	calendar-view-factory.h			\
	comp-util.h				\
	e-alarm-list.h				\
	e-cal-config.h				\
	e-cal-event.h				\
	e-cal-list-view.h			\
	e-cal-model-calendar.h			\
	e-cal-model.h				\
	e-calendar-selector.h			\
	e-calendar-view.h			\
	e-cell-date-edit-text.h			\
	e-date-time-list.h                      \
	e-day-view-layout.h			\
	e-day-view-main-item.h			\
	e-day-view-time-item.h			\
	e-day-view-top-item.h			\
	e-day-view.h				\
	e-meeting-attendee.h			\
	e-meeting-list-view.h			\
	e-meeting-store.h			\
	e-meeting-time-sel.h			\
	e-meeting-time-sel-item.h		\
	e-meeting-types.h			\
	e-meeting-utils.h			\
	e-select-names-editable.h		\
	e-select-names-renderer.h		\
	e-week-view-event-item.h		\
	e-week-view-layout.h			\
	e-week-view-main-item.h			\
	e-week-view-titles-item.h		\
	e-week-view.h				\
	e-timezone-entry.h			\
	gnome-cal.h				\
	goto.h					\
	itip-utils.h				\
	misc.h					\
	tag-calendar.h				\
	weekday-picker.h

search_files = tasktypes.xml memotypes.xml caltypes.xml

ruledir = $(privdatadir)
rule_DATA = $(search_files)

libevolution_calendar_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-DG_LOG_DOMAIN=\"calendar-gui\"			\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/misc			\
	-DEVOLUTION_RULEDIR=\"$(ruledir)\"		\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-DPREFIX=\""$(prefix)"\"			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(LIBSOUP_CFLAGS)				\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(CAMEL_CFLAGS)

ui_DATA = 					\
	e-itip-control.ui			\
	goto-dialog.ui

etspec_DATA =				\
	e-calendar-table.etspec		\
	e-meeting-time-sel.etspec	\
	e-cal-list-view.etspec		\
	e-memo-table.etspec

libevolution_calendar_la_SOURCES = \
	cal-editor-utils.c			\
	cal-editor-utils.h			\
	calendar-config.c			\
	calendar-config.h			\
	calendar-config-keys.h			\
	calendar-view.c				\
	calendar-view.h				\
	calendar-view-factory.c			\
	calendar-view-factory.h			\
	comp-util.c				\
	comp-util.h				\
	e-alarm-list.c				\
	e-alarm-list.h				\
	e-cal-component-preview.c		\
	e-cal-component-preview.h		\
	e-cal-config.c				\
	e-cal-config.h				\
	e-cal-event.c				\
	e-cal-event.h				\
	e-cal-model-calendar.c			\
	e-cal-model-calendar.h			\
	e-cal-model.c				\
	e-cal-model.h				\
	e-cal-list-view.c			\
	e-cal-list-view.h			\
	e-cal-model-memos.c			\
	e-cal-model-memos.h			\
	e-cal-model-tasks.c			\
	e-cal-model-tasks.h			\
	e-calendar-selector.c			\
	e-calendar-selector.h			\
	e-calendar-view.c			\
	e-calendar-view.h			\
	e-cell-date-edit-text.h			\
	e-cell-date-edit-text.c			\
	e-date-time-list.c			\
	e-date-time-list.h			\
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
	e-itip-control.c			\
	e-itip-control.h			\
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
	e-memo-list-selector.c			\
	e-memo-list-selector.h			\
	e-memo-table.c				\
	e-memo-table.h				\
	e-month-view.c				\
	e-month-view.h				\
	e-select-names-editable.c		\
	e-select-names-editable.h		\
	e-select-names-renderer.c		\
	e-select-names-renderer.h		\
	e-task-list-selector.c			\
	e-task-list-selector.h			\
	e-task-table.c				\
	e-task-table.h				\
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
	e-timezone-entry.c			\
	e-timezone-entry.h			\
	goto.c					\
	goto.h					\
	gnome-cal.c				\
	gnome-cal.h				\
	itip-utils.c				\
	itip-utils.h				\
	misc.c					\
	misc.h					\
	print.c					\
	print.h					\
	tag-calendar.c				\
	tag-calendar.h				\
	weekday-picker.c			\
	weekday-picker.h			\
	ea-calendar.c				\
	ea-calendar.h				\
	ea-calendar-helpers.c			\
	ea-calendar-helpers.h			\
	ea-cal-view.c				\
	ea-cal-view.h				\
	ea-cal-view-event.c			\
	ea-cal-view-event.h			\
	ea-day-view.c				\
	ea-day-view.h				\
	ea-day-view-main-item.c			\
	ea-day-view-main-item.h			\
	ea-day-view-cell.c			\
	ea-day-view-cell.h			\
	ea-week-view.c				\
	ea-week-view.h				\
	ea-week-view-main-item.c		\
	ea-week-view-main-item.h		\
	ea-week-view-cell.c			\
	ea-week-view-cell.h			\
	ea-jump-button.c			\
	ea-jump-button.h			\
	ea-gnome-calendar.c			\
	ea-gnome-calendar.h

libevolution_calendar_la_LIBADD =					\
	$(top_builddir)/composer/libcomposer.la				\
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la \
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la \
	$(top_builddir)/widgets/menus/libmenus.la			\
	$(top_builddir)/shell/libeshell.la				\
	$(top_builddir)/calendar/common/libevolution-calendarprivate.la	\
	$(top_builddir)/calendar/gui/dialogs/libcal-dialogs.la		\
	$(top_builddir)/calendar/importers/libevolution-calendar-importers.la \
	$(top_builddir)/widgets/e-timezone-dialog/libetimezonedialog.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/widgets/table/libetable.la			\
	$(top_builddir)/filter/libfilter.la				\
	$(top_builddir)/e-util/libeutil.la				\
	$(CAMEL_LIBS)							\
	$(LIBSOUP_LIBS)							\
	$(EVOLUTION_CALENDAR_LIBS)					\
	$(GNOME_PLATFORM_LIBS)

libevolution_calendar_la_LDFLAGS = $(NO_UNDEFINED)

@INTLTOOL_SERVER_RULE@

# GConf schemas

schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps_evolution_calendar.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

EXTRA_DIST =	 			\
	$(ui_DATA)			\
	$(schema_in_files)              \
	$(etspec_DATA)			\
	$(search_files) 

CLEANFILES = $(BUILT_SOURCES)

DISTCLEANFILES = $(schema_DATA)

if GCONF_SCHEMAS_INSTALL

if OS_WIN32
install-data-local:
	if test -z "$(DESTDIR)" ; then							\
		for p in $(schema_DATA) ; do						\
			(echo set GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE);	\
			 echo $(GCONFTOOL) --makefile-install-rule $$p) >_temp.bat;	\
			cmd /c _temp.bat; 						\
			rm _temp.bat;							\
		done									\
	fi
else
install-data-local:
	if test -z "$(DESTDIR)" ; then							\
		for p in $(schema_DATA) ; do						\
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) 		\
				$(GCONFTOOL) --makefile-install-rule $$p;		\
		done									\
	fi
endif

endif

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
