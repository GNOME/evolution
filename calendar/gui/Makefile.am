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
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl`   \
                $(srcdir)/../../composer/Evolution-Composer.idl
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl`   \
                $(top_srcdir)/calendar/idl/evolution-calendar.idl
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl`   \
                $(top_srcdir)/addressbook/gui/component/select-names/Evolution-Addressbook-SelectNames.idl

BUILT_SOURCES = $(IDL_GENERATED)

SUBDIRS = alarm-notify dialogs

help_base = $(datadir)/gnome/help/cal

bin_PROGRAMS = evolution-calendar

INCLUDES = 							\
	-DG_LOG_DOMAIN=\"calendar-gui\"				\
	-I$(top_builddir)/shell					\
	-I$(top_srcdir)/shell					\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/calendar				\
	-I$(top_srcdir)/calendar/cal-client			\
	-I$(top_builddir)/calendar/cal-client			\
	-I$(top_srcdir)/libical/src/libical			\
	-I$(top_builddir)/libical/src/libical			\
	-I$(top_srcdir)/addressbook/backend			\
	-I$(top_builddir)/addressbook/backend			\
	-I$(top_srcdir)/widgets					\
	-I$(includedir)						\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"			\
        -DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"        	\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"			\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"		\
	-DEVOLUTION_IMAGESDIR=\""$(datadir)"/images/evolution\" \
	$(EVOLUTION_CALENDAR_CFLAGS)

iconsdir = $(datadir)/images/evolution

gladedir   = $(datadir)/evolution/glade
glade_DATA = 					\
	e-itip-control.glade			\
	goto-dialog.glade

etspecdir = $(datadir)/evolution/etspec/
etspec_DATA = e-calendar-table.etspec e-meeting-time-sel.etspec

evolution_calendar_SOURCES =			\
	$(IDL_GENERATED)			\
	cal-search-bar.c			\
	cal-search-bar.h			\
	calendar-config.c			\
	calendar-config.h			\
	calendar-commands.c			\
	calendar-commands.h			\
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
	config-control-factory.c		\
	config-control-factory.h		\
	control-factory.c			\
	control-factory.h			\
	component-factory.c			\
	component-factory.h			\
	e-calendar-table.h			\
	e-calendar-table.c			\
	e-cell-date-edit-text.h			\
	e-cell-date-edit-text.c			\
	e-comp-editor-registry.c		\
	e-comp-editor-registry.h		\
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
	itip-control-factory.c			\
	itip-control-factory.h			\
	itip-utils.c				\
	itip-utils.h				\
	main.c					\
	misc.c					\
	misc.h					\
	print.c					\
	print.h					\
	tag-calendar.c				\
	tag-calendar.h				\
	tasks-control-factory.c			\
	tasks-control-factory.h			\
	tasks-control.c				\
	tasks-control.h				\
	tasks-migrate.c				\
	tasks-migrate.h				\
	weekday-picker.c			\
	weekday-picker.h

evolution_calendar_LDADD =								\
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

evolution_calendar_LDFLAGS = -export-dynamic

oafdir = $(datadir)/oaf
oaf_in_files = \
	GNOME_Evolution_Calendar.oaf.in

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST =	 			\
	$(glade_DATA)			\
	$(etspec_DATA)			\
	$(oaf_in_files)

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(help_base)/C
	$(mkinstalldirs) $(Conduitsdir)

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution-calendar.pure

evolution-calendar.pure: evolution-calendar
	@rm -f evolution-calendar.pure
	$(PLINK) $(evolution_calendar_LDFLAGS) $(evolution_calendar_OBJECTS) $(evolution_calendar_LDADD) $(LIBS)

endif

CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
