SUBDIRS = doc

idldir = $(datadir)/idl
idl_DATA = GnomeCal.idl

gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA = gnomecal.gnorba

help_base = $(datadir)/gnome/help/cal

CFLAGS += -Wall -Wunused -Wpointer-arith -Wmissing-declarations

INCLUDES = 			\
	-I$(includedir)					\
	$(GNOME_INCLUDEDIR)				\
	$(GNOME_CONDUIT_INCLUDEDIR)				\
	$(PISOCK_INCLUDEDIR)                  \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"

bin_PROGRAMS = gnomecal @CALSYNC@
EXTRA_PROGRAMS = calendar-pilot-sync

GNOMECAL_CORBA_GENERATED =	\
	GnomeCal.h		\
	GnomeCal.c		\
	GnomeCal-common.c	\
	GnomeCal-skels.c	\
	GnomeCal-stubs.c

$(GNOMECAL_CORBA_GENERATED): my_gnomecal_idl

my_gnomecal_idl: GnomeCal.idl
	orbit-idl $(srcdir)/GnomeCal.idl
	touch my_gnomecal_idl

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



LINK_FLAGS = \
	$(GNOME_LIBDIR) \
	$(GNOMEGNORBA_LIBS) \
	$(INTLLIBS) ../libversit/libversit.a

calendar_pilot_sync_LDADD =	\
	$(PISOCK_LIBS)		\
	$(LINK_FLAGS)


#calendar_conduit
calendar_conduitsdir=$(libdir)/gnome-pilot/conduits

calendar_conduits_LTLIBRARIES = @CALCONDUITLIB@

EXTRA_LTLIBRARIES = libcalendar_conduit.la

GNOME_CONDUIT_LIBS=

libcalendar_conduit_la_LIBADD = \
	$(GNOME_CONDUIT_LIBS) \
	$(GNOME_LIBDIR) \
	$(GNOME_LIBS)

libcalendar_conduit_la_SOURCES = \
	calendar-conduit.c

#libcalendar_conduit_la_OBJECTS = \
#	calendar-conduit.o

#gncal_LDADD = $(LINK_FLAGS)

gnomecal_LDADD = $(LINK_FLAGS)

EXTRA_DIST = \
	gncal.desktop	\
	bell.xpm	\
	recur.xpm	\
	$(idl_DATA)	\
	$(gnorba_DATA)


Productivitydir = $(datadir)/gnome/apps/Applications

Productivity_DATA = gncal.desktop

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(help_base)/C
