SUBDIRS = dialogs

help_base = $(datadir)/gnome/help/cal

bin_PROGRAMS = evolution-calendar

CPPFLAGS =					\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"

INCLUDES = 						\
	-DG_LOG_DOMAIN=\"calendar-gui\"			\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/libical/src/libical		\
	-I$(top_srcdir)/widgets				\
	-I$(includedir)					\
	$(BONOBO_VFS_GNOME_CFLAGS)			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"

LINK_FLAGS =							\
	$(BONOBO_VFS_GNOME_LIBS)				\
	$(INTLLIBS)						\
	dialogs/libcal-dialogs.a				\
	$(top_builddir)/calendar/cal-client/libcal-client.la	\
	$(top_builddir)/calendar/cal-util/libcal-util.la	\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/widgets/e-paned/libepaned.a		\
	$(top_builddir)/widgets/e-text/libetext.a		\
	$(top_builddir)/widgets/e-table/libetable.a		\
	$(top_builddir)/libversit/libversit.la			\
	$(top_builddir)/libical/src/libical/libical.a

gladedir   = $(datadir)/evolution/glade

glade_DATA = 					\
	event-editor-dialog.glade

glade_messages = event-editor-dialog.glade.h

evolution_calendar_SOURCES =			\
	alarm.c					\
	alarm.h					\
	calendar-commands.c			\
	calendar-commands.h			\
	calendar-model.c			\
	calendar-model.h			\
	control-factory.c			\
	control-factory.h			\
	component-factory.c			\
	component-factory.h			\
	e-day-view-main-item.c			\
	e-day-view-main-item.h			\
	e-day-view-time-item.c			\
	e-day-view-time-item.h			\
	e-day-view-top-item.c			\
	e-day-view-top-item.h			\
	e-day-view.c				\
	e-day-view.h				\
	e-week-view-event-item.c		\
	e-week-view-event-item.h		\
	e-week-view-main-item.c			\
	e-week-view-main-item.h			\
	e-week-view-titles-item.c		\
	e-week-view-titles-item.h		\
	e-week-view.c				\
	e-week-view.h				\
	event-editor.c				\
	event-editor.h				\
	getdate.y				\
	gncal-todo.c				\
	gncal-todo.h				\
	gnome-month-item.c			\
	gnome-month-item.h			\
	gnome-cal.c				\
	gnome-cal.h				\
	goto.c					\
	layout.c				\
	layout.h				\
	main.c					\
	mark.c					\
	mark.h					\
	popup-menu.c				\
	popup-menu.h				\
	print.c					\
	print.h					\
	prop.c

evolution_calendar_LDADD =					\
	$(top_builddir)/shell/libeshell.a			\
	$(EXTRA_GNOME_LIBS)					\
	$(BONOBO_HTML_GNOME_LIBS)				\
	$(top_builddir)/widgets/e-text/libetext.a		\
	$(top_builddir)/e-util/libeutil.la			\
	$(LINK_FLAGS)

evolution_calendar_LDFLAGS = `gnome-config --libs gdk_pixbuf`

if USING_OAF
oafdir = $(datadir)/oaf
oaf_DATA = calendar-control.oafinfo evolution-calendar.oafinfo
else
gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA = calendar-control.gnorba evolution-calendar.gnorba
endif

OAF_GNORBA_EXTRAS = calendar-control.oafinfo evolution-calendar.oafinfo calendar-control.gnorba evolution-calendar.gnorba
EXTRA_DIST =	 			\
	$(OAF_GNORBA_EXTRAS)		\
	$(glade_DATA)			\
	bell.xpm			\
	recur.xpm			\
	jump.xpm			\
	dayview.xpm			\
	workweekview.xpm		\
	weekview.xpm			\
	monthview.xpm			\
	yearview.xpm			\
	calendar-control.gnorba		\
	calendar-control.oafinfo


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
