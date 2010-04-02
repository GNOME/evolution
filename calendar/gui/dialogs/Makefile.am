noinst_LTLIBRARIES = libcal-dialogs.la

libcal_dialogs_la_CPPFLAGS = 						\
	$(AM_CPPFLAGS)							\
	-DG_LOG_DOMAIN=\"calendar-gui\"					\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/widgets						\
	-I$(top_builddir)						\
	-I$(top_srcdir)/calendar					\
	-I$(top_builddir)/shell						\
	-I$(top_srcdir)/shell						\
	-I$(top_srcdir)/widgets/misc					\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"				\
	-DEVOLUTION_ICONDIR=\""$(icondir)"\"				\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"				\
	-DPREFIX=\""$(prefix)"\"					\
	$(GNOME_PLATFORM_CFLAGS)					\
	$(EVOLUTION_CALENDAR_CFLAGS)					\
	$(CAMEL_CFLAGS)

ecalendarincludedir = $(privincludedir)/calendar/gui/dialogs

ecalendarinclude_HEADERS =	\
	comp-editor-page.h	\
	comp-editor.h		\
	alarm-dialog.h		\
	alarm-list-dialog.h	\
	cal-prefs-dialog.h	\
	calendar-setup.h	\
	cancel-comp.h		\
	changed-comp.h		\
	comp-editor-util.h	\
	copy-source-dialog.h	\
	delete-comp.h		\
	delete-error.h		\
	e-delegate-dialog.h	\
	e-send-options-utils.h  \
	event-editor.h		\
	event-page.h		\
	memo-editor.h		\
	memo-page.h		\
	recurrence-page.h	\
	recur-comp.h		\
	save-comp.h		\
	schedule-page.h		\
	select-source-dialog.h	\
	send-comp.h		\
	task-editor.h		\
	task-details-page.h	\
	task-page.h		

libcal_dialogs_la_LIBADD =					\
	$(top_builddir)/addressbook/util/libeabutil.la		\
	$(CAMEL_LIBS)						\
	$(GNOME_PLATFORM_LIBS)

libcal_dialogs_la_SOURCES =	\
	$(IDL_GENERATED)	\
	alarm-dialog.c		\
	alarm-dialog.h		\
	alarm-list-dialog.c	\
	alarm-list-dialog.h	\
	cal-prefs-dialog.c	\
	cal-prefs-dialog.h	\
	calendar-setup.c	\
	calendar-setup.h	\
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
	copy-source-dialog.c	\
	copy-source-dialog.h	\
	delete-comp.c		\
	delete-comp.h		\
	delete-error.c		\
	delete-error.h		\
	e-delegate-dialog.c	\
	e-delegate-dialog.h	\
	e-send-options-utils.c  \
	e-send-options-utils.h  \
	event-editor.c		\
	event-editor.h		\
	event-page.c		\
	event-page.h		\
	memo-editor.c		\
	memo-editor.h		\
	memo-page.c		\
	memo-page.h		\
	recurrence-page.c	\
	recurrence-page.h	\
	recur-comp.c		\
	recur-comp.h		\
	save-comp.c		\
	save-comp.h		\
	schedule-page.c		\
	schedule-page.h		\
	select-source-dialog.c	\
	select-source-dialog.h	\
	send-comp.c		\
	send-comp.h		\
	task-editor.c		\
	task-editor.h		\
	task-details-page.c	\
	task-details-page.h	\
	task-page.c		\
	task-page.h		

ui_DATA =				\
	alarm-dialog.ui		\
	alarm-list-dialog.ui		\
	cal-prefs-dialog.ui		\
	e-delegate-dialog.ui		\
	event-page.ui		\
	memo-page.ui			\
	recurrence-page.ui		\
	schedule-page.ui		\
	task-details-page.ui		\
	task-page.ui

CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

EXTRA_DIST =			\
	$(ui_DATA)

-include $(top_srcdir)/git.mk
