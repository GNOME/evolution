IDLS = $(top_srcdir)/addressbook/gui/component/select-names/Evolution-Addressbook-SelectNames.idl

IDL_GENERATED =                 			\
	Evolution-Addressbook-SelectNames.h		\
	Evolution-Addressbook-SelectNames-common.c	\
	Evolution-Addressbook-SelectNames-skels.c	\
	Evolution-Addressbook-SelectNames-stubs.c

$(IDL_GENERATED): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl`   \
                $(top_srcdir)/addressbook/gui/component/select-names/Evolution-Addressbook-SelectNames.idl

BUILT_SOURCES = $(IDL_GENERATED)

INCLUDES = 								\
	-DG_LOG_DOMAIN=\"calendar-gui\"					\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/calendar					\
	-I$(top_srcdir)/calendar/cal-client				\
	-I$(top_builddir)/calendar/cal-client				\
	-I$(top_srcdir)/libical/src/libical				\
	-I$(top_builddir)/libical/src/libical				\
	-I$(top_builddir)/shell						\
	-I$(top_srcdir)/shell						\
	-I$(top_srcdir)/addressbook/backend				\
	-I$(top_builddir)/addressbook/backend				\
	-I$(includedir)							\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"				\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"				\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"				\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"			\
        -DGNOMELOCALEDIR=\""$(localedir)"\"				\
	$(EVOLUTION_CALENDAR_CFLAGS)

noinst_LIBRARIES = libcal-dialogs.a

libcal_dialogs_a_SOURCES =	\
	$(IDL_GENERATED)	\
	alarm-options.c		\
	alarm-options.h		\
	alarm-page.c		\
	alarm-page.h		\
	cal-prefs-dialog.c	\
	cal-prefs-dialog.h	\
	cancel-comp.c		\
	cancel-comp.h		\
	changed-comp.c		\
	changed-comp.h		\
	comp-editor.c		\
	comp-editor.h		\
	comp-editor-page.c	\
	comp-editor-page.h	\
	comp-editor-util.c	\
	comp-editor-util.h	\
	delete-comp.c		\
	delete-comp.h		\
	e-delegate-dialog.c	\
	e-delegate-dialog.h	\
	event-editor.c		\
	event-editor.h		\
	event-page.c		\
	event-page.h		\
	meeting-page.c		\
	meeting-page.h		\
	recurrence-page.c	\
	recurrence-page.h	\
	recur-comp.c		\
	recur-comp.h		\
	save-comp.c		\
	save-comp.h		\
	schedule-page.c		\
	schedule-page.h		\
	send-comp.c		\
	send-comp.h		\
	task-editor.c		\
	task-editor.h		\
	task-details-page.c	\
	task-details-page.h	\
	task-page.c		\
	task-page.h

iconsdir = $(datadir)/images/evolution

gladedir = $(datadir)/evolution/glade
glade_DATA =				\
	alarm-options.glade		\
	alarm-page.glade		\
	cal-prefs-dialog.glade		\
	e-delegate-dialog.glade		\
	event-page.glade		\
	meeting-page.glade		\
	recurrence-page.glade		\
	schedule-page.glade		\
	task-details-page.glade		\
	task-page.glade

etspecdir = $(datadir)/evolution/etspec/
etspec_DATA = meeting-page.etspec

CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

EXTRA_DIST =			\
	$(glade_DATA)		\
	$(etspec_DATA)
