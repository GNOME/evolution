help_base = $(datadir)/gnome/help/cal

#if HAVE_GNOME_PILOT
#extra_pilot_bins = 				\
#		 calendar-conduit-control-applet	\
#		 calendar-pilot-sync
#endif


INCLUDES = 						\
	-I$(includedir)					\
	$(BONOBO_VFS_GNOME_CFLAGS)			\
	$(GNOME_CONDUIT_INCLUDEDIR)			\
	$(PISOCK_INCLUDEDIR)                  		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/libical/src/libical		\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"


LINK_FLAGS = \
	$(BONOBO_VFS_GNOME_LIBS) \
	$(INTLLIBS) \
	$(top_builddir)/calendar/cal-util/libcalutil.la \
	$(top_builddir)/e-util/libeutil.la \
	$(top_builddir)/widgets/e-text/libetext.a \
	$(top_builddir)/libversit/libversit.la \
	$(top_builddir)/libical/src/libical/libical.la \
	$(top_builddir)/calendar/cal-client/libcal-client.la

noinst_LIBRARIES = libcalendarwidget.a

libcalendarwidget_a_SOURCES =			\
	alarm.c					\
	alarm.h					\
	e-day-view-main-item.c			\
	e-day-view-main-item.h			\
	e-day-view-time-item.c			\
	e-day-view-time-item.h			\
	e-day-view-top-item.c			\
	e-day-view-top-item.h			\
	e-day-view.c				\
	e-day-view.h				\
	eventedit.c				\
	eventedit.h				\
	getdate.y				\
	gncal-day-panel.c			\
	gncal-day-panel.h			\
	gncal-day-view.c			\
	gncal-day-view.h			\
	gncal-full-day.c			\
	gncal-full-day.h			\
	gncal-week-view.c			\
	gncal-week-view.h			\
	gncal-todo.c				\
	gncal-todo.h				\
	gnome-month-item.c			\
	gnome-month-item.h			\
	gnome-cal.c				\
	gnome-cal.h				\
	goto.c					\
	layout.c				\
	layout.h				\
	mark.c					\
	mark.h					\
	month-view.c				\
	month-view.h				\
	popup-menu.c				\
	popup-menu.h				\
	prop.c					\
	quick-view.c				\
	quick-view.h				\
	todo-conduit.h				\
	view-utils.h				\
	view-utils.c				\
	year-view.c				\
	year-view.h				\
	calendar-commands.c			\
	calendar-commands.h


bin_PROGRAMS = evolution-calendar
noinst_PROGRAMS = test-calendar-widget
test_calendar_widget_SOURCES = main.c
test_calendar_widget_INCLUDES =		\
	$(INCLUDES)			\
	-DG_LOG_DOMAIN=\"test-calendar-widget\"
test_calendar_widget_LDADD = \
	libcalendarwidget.a \
	$(LINK_FLAGS)

evolution_calendar_SOURCES = evolution-calendar-control.c

evolution_calendar_LDADD =					\
	$(EXTRA_GNOME_LIBS)					\
	$(BONOBO_HTML_GNOME_LIBS)				\
	$(top_builddir)/calendar/gui/libcalendarwidget.a	\
	$(top_builddir)/widgets/e-minicard/libeminicard.a	\
	$(top_builddir)/widgets/e-table/libetable.a		\
	$(top_builddir)/widgets/e-text/libetext.a		\
	$(top_builddir)/e-util/libeutil.la			\
	$(LINK_FLAGS)

evolution_calendar_LDFLAGS = `gnome-config --libs gdk_pixbuf`

gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA = calendar-control.gnorba

#calendar_pilot_sync_SOURCES =	\
#	 GnomeCal-common.c	\
#	 GnomeCal-stubs.c	\
#	 alarm.c			\
#	 calendar-pilot-sync.c	\
#	 calendar.c		\
#	 calendar.h


#calendar_pilot_sync_LDADD =	\
#	 $(PISOCK_LIBDIR) $(PISOCK_LIBS)		\
#	 $(LINK_FLAGS)

#if HAVE_GNOME_PILOT
##calendar_conduit
#calendar_conduitsdir=$(libdir)/gnome-pilot/conduits
#
#calendar_conduits_LTLIBRARIES = libcalendar_conduit.la
#endif 

#libcalendar_conduit_la_SOURCES =		\
#	 GnomeCal-common.c			\
#	 GnomeCal-stubs.c			\
#	 calendar-conduit.h			\
#	 calendar-conduit.c			\
#	 alarm.c					\
#	 calendar.c

#libcalendar_conduit_la_LDFLAGS = \
#	 -rpath $(libdir)

#libcalendar_conduit_la_LIBADD = \
#	 ../libversit/libversit.la \
#	 $(PISOCK_LIBDIR) $(PISOCK_LIBS) \
#	 $(GNOME_LIBDIR) \
#	 $(GNOME_LIBS)

#calendar_conduit_control_applet_SOURCES = \
#	 calendar-conduit-control-applet.c

#calendar_conduit_control_applet_LDADD = \
#	 $(CAPPLET_LIBS) \
#	 $(GNOME_LIBDIR) \
#	 $(GNOME_CAPPLET_LIBS) \
#	 $(PISOCK_LIBDIR) $(PISOCK_LIBS) \
#	 $(GNOMEUI_LIBS) \
#	 $(GNOME_PILOT_LIBS) \
#	 $(INTLLIBS) 

#todo_conduit
#todo_conduitsdir=$(libdir)/gnome-pilot/conduits

#todo_conduits_LTLIBRARIES = libtodo_conduit.la

#libtodo_conduit_la_LDFLAGS = \
#	-rpath $(libdir)

#libtodo_conduit_la_LIBADD = \
#	$(GNOME_LIBDIR) \
#	$(GNOME_LIBS)

#libtodo_conduit_la_SOURCES = \
#	todo-conduit.c

#todo-conduit.c:	GnomeCal.h

#todo_conduit_control_applet_SOURCES = \
#	todo-conduit-control-applet.c

#todo_conduit_control_applet_LDADD = \
#	$(CAPPLET_LIBS) \
#	$(GNOME_LIBDIR) \
#	$(GNOME_CAPPLET_LIBS) \
#	$(PISOCK_LIBDIR) $(PISOCK_LIBS) \
#	$(GNOMEUI_LIBS) \
#	$(GNOME_PILOT_LIBS) \
#	$(INTLLIBS) 

#if HAVE_GNOME_PILOT
#ccenterdir = $(datadir)/control-center
#Rootdir = $(ccenterdir)
#PalmPilotdir = $(ccenterdir)/Peripherals/PalmPilot
#Conduitsdir = $(PalmPilotdir)/Conduits
#
#Conduits_DATA = \
#	 calendar-conduit-control-applet.desktop 
##	todo-conduit-control-applet.desktop 
#
#Conduits_seconddir = $(datadir)/gnome/apps/Settings/Peripherals/PalmPilot/Conduits/
#Conduits_second_DATA = $(Conduits_DATA)
#endif

EXTRA_DIST = \
	gncal.desktop	\
	bell.xpm	\
	recur.xpm

#	gnome-calendar-conduit.png
#	todo-conduit-control-applet.desktop
#	$(idl_DATA)
#	$(gnorba_DATA)
#	calendar-conduit-control-applet.desktop

#appicondir = $(datadir)/pixmaps
#appicon_DATA = gnome-calendar-conduit.png

Productivitydir = $(datadir)/gnome/apps/Applications

Productivity_DATA = gncal.desktop

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(help_base)/C
#	$(mkinstalldirs) $(Conduitsdir)
