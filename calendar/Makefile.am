SUBDIRS = doc

idldir = $(datadir)/idl
idl_DATA = 			\
	GnomeCal.idl		\
	gnome-calendar.idl

gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA =			\
	gnomecal.gnorba		\
	tlacuache.gnorba

help_base = $(datadir)/gnome/help/cal

if HAVE_GNOME_PILOT
extra_pilot_bins = 				\
		calendar-conduit-control-applet	\
		calendar-pilot-sync
endif

bin_PROGRAMS = gnomecal tlacuache $(extra_pilot_bins)

INCLUDES = 						\
	-I$(includedir)					\
	$(GNOME_INCLUDEDIR)				\
	$(GNOME_CONDUIT_INCLUDEDIR)			\
	$(PISOCK_INCLUDEDIR)                  		\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"

GNOMECAL_CORBA_GENERATED =	\
	GnomeCal.h		\
	GnomeCal.c		\
	GnomeCal-common.c	\
	GnomeCal-skels.c	\
	GnomeCal-stubs.c

$(GNOMECAL_CORBA_GENERATED): GnomeCal.idl
	orbit-idl $(srcdir)/GnomeCal.idl

corba-cal.c		\
corba-cal.h		\
corba-cal-factory.c	\
corba-cal-factory.h: GnomeCal.h

gnomecal_SOURCES = \
	GnomeCal-common.c	\
	GnomeCal-skels.c	\
	GnomeCal.h		\
	alarm.c			\
	alarm.h			\
	calendar.c		\
	calendar.h		\
	calendar-conduit.h	\
	calobj.c		\
	calobj.h		\
	eventedit.c		\
	eventedit.h		\
	getdate.y		\
	corba-cal.c		\
	corba-cal.h		\
	corba-cal-factory.c	\
	corba-cal-factory.h	\
	gncal-day-panel.c	\
	gncal-day-panel.h	\
	gncal-day-view.c	\
	gncal-day-view.h	\
	gncal-full-day.c	\
	gncal-full-day.h	\
	gncal-week-view.c	\
	gncal-week-view.h	\
	gncal-todo.c		\
	gncal-todo.h		\
	gnome-month-item.c	\
	gnome-month-item.h	\
	gnome-cal.c		\
	gnome-cal.h		\
	html-month.c		\
	goto.c			\
	layout.c		\
	layout.h		\
	main.c			\
	main.h			\
	mark.c			\
	mark.h			\
	month-view.c		\
	month-view.h		\
	popup-menu.c		\
	popup-menu.h		\
	prop.c			\
	quick-view.c		\
	quick-view.h		\
	timeutil.c		\
	timeutil.h		\
	todo-conduit.h		\
	view-utils.h		\
	view-utils.c		\
	year-view.c		\
	year-view.h

calendar_pilot_sync_SOURCES =	\
	GnomeCal-common.c	\
	GnomeCal-stubs.c	\
	alarm.c			\
	calendar-pilot-sync.c	\
	calobj.c		\
	calobj.h		\
	calendar.c		\
	calendar.h		\
	timeutil.c		\
	timeutil.h

GNOME_CALENDAR_CORBA_GENERATED =	\
	gnome-calendar.h		\
	gnome-calendar-common.c		\
	gnome-calendar-skels.c		\
	gnome-calendar-stubs.c

$(GNOME_CALENDAR_CORBA_GENERATED): gnome-calendar.idl
	orbit-idl -I`$(GNOME_CONFIG) --datadir`/idl $(srcdir)/gnome-calendar.idl

tlacuache_SOURCES =				\
	$(GNOME_CALENDAR_CORBA_GENERATED)	\
	alarm.c	\
	alarm.h	\
	cal.c					\
	cal.h					\
	cal-backend.c				\
	cal-backend.h				\
	cal-common.h				\
	cal-factory.c				\
	cal-factory.h				\
	calobj.c				\
	calobj.h				\
	job.c					\
	job.h					\
	timeutil.c				\
	timeutil.h				\
	tlacuache.c

LINK_FLAGS = \
	$(BONOBO_VFS_GNOME_LIBS) \
	$(INTLLIBS) \
	../libversit/libversit.la

tlacuache_INCLUDES =			\
	$(INCLUDES)			\
	-DG_LOG_DOMAIN=\"tlacuache\"

tlacuache_LDADD =			\
	$(BONOBO_VFS_GNOME_LIBS)	\
	../libversit/libversit.la

calendar_pilot_sync_LDADD =	\
	$(PISOCK_LIBDIR) $(PISOCK_LIBS)		\
	$(LINK_FLAGS)

if HAVE_GNOME_PILOT
#calendar_conduit
calendar_conduitsdir=$(libdir)/gnome-pilot/conduits

calendar_conduits_LTLIBRARIES = libcalendar_conduit.la
endif 

libcalendar_conduit_la_SOURCES = \
	GnomeCal-common.c	\
	GnomeCal-stubs.c	\
	calendar-conduit.c	\
	calobj.c 		\
	alarm.c			\
	calendar.c 		\
	timeutil.c

calendar-conduit.c: my_gnomecal_idl

libcalendar_conduit_la_LDFLAGS = \
	-rpath $(libdir)

libcalendar_conduit_la_LIBADD = \
	../libversit/libversit.la \
	$(PISOCK_LIBDIR) $(PISOCK_LIBS) \
	$(GNOME_LIBDIR) \
	$(GNOME_LIBS)

calendar_conduit_control_applet_SOURCES = \
	calendar-conduit-control-applet.c

calendar_conduit_control_applet_LDADD = \
	$(CAPPLET_LIBS) \
	$(GNOME_LIBDIR) \
        $(GNOME_CAPPLET_LIBS) \
        $(PISOCK_LIBDIR) $(PISOCK_LIBS) \
        $(GNOMEUI_LIBS) \
	$(GNOME_PILOT_LIBS) \
        $(INTLLIBS) 

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

gnomecal_INCLUDES =			\
	$(INCLUDES)			\
	-DG_LOG_DOMAIN=\"gnomecal\"

gnomecal_LDADD = $(LINK_FLAGS)

if HAVE_GNOME_PILOT
ccenterdir = $(datadir)/control-center
Rootdir = $(ccenterdir)
PalmPilotdir = $(ccenterdir)/Peripherals/PalmPilot
Conduitsdir = $(PalmPilotdir)/Conduits

Conduits_DATA = \
	calendar-conduit-control-applet.desktop 
#	todo-conduit-control-applet.desktop 

Conduits_seconddir = $(datadir)/gnome/apps/Settings/Peripherals/PalmPilot/Conduits/
Conduits_second_DATA = $(Conduits_DATA)
endif

EXTRA_DIST = \
	gncal.desktop	\
	calendar-conduit-control-applet.desktop \
	bell.xpm	\
	recur.xpm	\
	$(idl_DATA)	\
	$(gnorba_DATA)  \
	gnome-calendar-conduit.png 

#	todo-conduit-control-applet.desktop \


appicondir = $(datadir)/pixmaps
appicon_DATA = gnome-calendar-conduit.png

Productivitydir = $(datadir)/gnome/apps/Applications

Productivity_DATA = gncal.desktop

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(help_base)/C
	$(mkinstalldirs) $(Conduitsdir)
