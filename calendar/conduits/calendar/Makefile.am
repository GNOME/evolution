INCLUDES = 					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/calendar		\
	-I$(top_builddir)/calendar/cal-client	\
	-I$(top_srcdir)/libical/src/libical	\
	-I$(top_builddir)/libical/src/libical	\
	-I$(top_srcdir)/e-util			\
	-I$(top_builddir)/e-util		\
	$(EVOLUTION_CALENDAR_CONDUIT_CFLAGS)

# Calendar Conduit
e_calendar_conduitsdir=$(libdir)/gnome-pilot/conduits
e_calendar_conduits_LTLIBRARIES = libecalendar_conduit.la

libecalendar_conduit_la_SOURCES = 	\
	calendar-conduit.c

libecalendar_conduit_la_LDFLAGS = -module -avoid-version
libecalendar_conduit_la_LIBADD = 					\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/calendar/cal-client/libcal-client-static.la	\
	$(top_builddir)/calendar/cal-util/libcal-util-static.la		\
	$(top_builddir)/libversit/libversit.la				\
	$(top_builddir)/libical/src/libical/libical-static.la		\
	$(top_builddir)/libwombat/libwombat-static.la		 	\
	$(top_builddir)/e-util/libeconduit.la	 			\
	$(EVOLUTION_CALENDAR_CONDUIT_LIBS)

e-calendar.conduit: e-calendar.conduit.in Makefile
	sed -e 's^\@prefix\@^$(prefix)^g' 				\
	    -e 's^\@datadir\@^$(datadir)^g' 				\
	    < $(srcdir)/e-calendar.conduit.in > e-calendar.conduit.tmp 	\
	&& mv e-calendar.conduit.tmp e-calendar.conduit

Conduitdir = $(datadir)/gnome-pilot/conduits/
Conduit_DATA = e-calendar.conduit

EXTRA_DIST = \
	e-calendar.conduit.in

install-data-local:
	$(mkinstalldirs) $(Conduitdir)
