## CORBA stuff

IDLS =												   	\
	$(top_srcdir)/composer/Evolution-Composer.idl						  	\
	$(top_srcdir)/calendar/idl/evolution-calendar.idl						\
	$(top_srcdir)/addressbook/gui/component/select-names/Evolution-Addressbook-SelectNames.idl

IDL_GENERATED =                 			\
        Evolution-Composer.h              		\
        Evolution-Composer-common.c       		\
        Evolution-Composer-skels.c        		\
        Evolution-Composer-stubs.c			\
	Evolution-Addressbook-SelectNames.h		\
	Evolution-Addressbook-SelectNames-common.c	\
	Evolution-Addressbook-SelectNames-skels.c	\
	Evolution-Addressbook-SelectNames-stubs.c	\
	evolution-calendar.h				\
	evolution-calendar-common.c			\
	evolution-calendar-skels.c			\
	evolution-calendar-stubs.c

$(IDL_GENERATED): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES)   \
                $(srcdir)/../../composer/Evolution-Composer.idl
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES)   \
                $(top_srcdir)/calendar/idl/evolution-calendar.idl
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES)   \
                $(top_srcdir)/addressbook/gui/component/select-names/Evolution-Addressbook-SelectNames.idl

BUILT_SOURCES = $(IDL_GENERATED)

SUBDIRS = alarm-notify dialogs

component_LTLIBRARIES = libevolution-calendar.la

INCLUDES =										\
	-DG_LOG_DOMAIN=\"calendar-gui\"							\
	-I$(top_builddir)/shell								\
	-I$(top_srcdir)/shell								\
	-I$(top_srcdir)									\
	-I$(top_srcdir)/calendar							\
	-I$(top_srcdir)/calendar/cal-client						\
	-I$(top_builddir)/calendar/cal-client						\
	-I$(top_srcdir)/libical/src/libical						\
	-I$(top_builddir)/libical/src/libical						\
	-I$(top_srcdir)/addressbook/backend						\
	-I$(top_builddir)/addressbook/backend						\
	-I$(top_srcdir)/widgets								\
	-I$(includedir)									\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"						\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"						\
        -DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"					\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"						\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"					\
	-DEVOLUTION_IMAGESDIR=\""$(datadir)"/evolution-$(BASE_VERSION)/images\"		\
	-DEVOLUTION_GALVIEWSDIR=\""$(datadir)"/evolution-$(BASE_VERSION)/views\"	\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)\""					\
	$(EVOLUTION_CALENDAR_CFLAGS)

iconsdir = $(datadir)/evolution-$(BASE_VERSION)/images

gladedir   = $(datadir)/evolution-$(BASE_VERSION)/glade
glade_DATA = 					\
	e-itip-control.glade			\
	goto-dialog.glade

etspecdir = $(datadir)/evolution-$(BASE_VERSION)/etspec/
etspec_DATA = e-calendar-table.etspec e-meeting-time-sel.etspec

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
	calendar-model.c			\
	calendar-model.h			\
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
	e-meeting-model.c			\
	e-meeting-model.h			\
	e-meeting-time-sel.c			\
	e-meeting-time-sel.h			\
	e-meeting-time-sel-item.c		\
	e-meeting-time-sel-item.h		\
	e-meeting-types.h			\
	e-meeting-utils.c			\
	e-meeting-utils.h			\
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
	tasks-migrate.c				\
	tasks-migrate.h				\
	weekday-picker.c			\
	weekday-picker.h

libevolution_calendar_la_LIBADD =							\
	alarm-notify/libalarm.a								\
	$(DB3_LDADD)									\
	$(top_builddir)/widgets/menus/libmenus.la					\
	$(top_builddir)/shell/libeshell.la						\
	$(top_builddir)/calendar/cal-client/libcal-client.la				\
	$(top_builddir)/calendar/cal-util/libcal-util.la				\
	$(top_builddir)/e-util/libeutil.la						\
	$(top_builddir)/libical/src/libical/libical-evolution.la			\
	$(top_builddir)/libwombat/libwombat.la						\
	$(top_builddir)/addressbook/backend/ebook/libebook.la				\
	$(top_builddir)/camel/libcamel.la						\
	$(top_builddir)/libversit/libversit.a						\
	$(top_builddir)/e-util/ename/libename.la					\
	dialogs/libcal-dialogs.a							\
	$(top_builddir)/widgets/e-timezone-dialog/libetimezonedialog.a			\
	$(top_builddir)/widgets/misc/libemiscwidgets.a					\
	$(EVOLUTION_CALENDAR_LIBS)

libevolution_calendar_la_LDFLAGS =						\
	-export-dynamic -avoid-version

server_in_files = GNOME_Evolution_Calendar.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@COMPONENTDIR\@|$(componentdir)|" $< > $@

@INTLTOOL_SERVER_RULE@

EXTRA_DIST =	 			\
	$(glade_DATA)			\
	$(etspec_DATA)			\
	$(server_in_files)

CLEANFILES = $(BUILT_SOURCES)

install-data-local:
	$(mkinstalldirs) $(Conduitsdir)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
