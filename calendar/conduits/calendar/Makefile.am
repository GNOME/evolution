INCLUDES = 					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/calendar		\
	-I$(top_srcdir)/libical/src/libical	\
	-I$(top_builddir)/libical/src/libical	\
	-I$(top_builddir)/e-util		\
	$(BONOBO_VFS_GNOME_CFLAGS)		\
	$(PISOCK_INCLUDEDIR)			\
	$(GNOME_PILOT_CFLAGS)

# Calendar Capplet
bin_PROGRAMS = e-calendar-conduit-control-applet

e_calendar_conduit_control_applet_SOURCES = calendar-conduit-control-applet.c

e_calendar_conduit_control_applet_LDADD = \
		$(CAPPLET_LIBS)  \
		$(GNOME_LIBDIR) \
		$(GNOME_PILOT_LIBS) \
		$(PISOCK_LIBDIR) $(PISOCK_LIBS) \
		$(GNOME_CAPPLET_LIBS) \
		$(BONOBO_VFS_GNOME_LIBS) \
		$(GNOME_XML_LIB) \
	        $(GNOMEUI_LIBS) \
                $(INTLLIBS)

# Calendar Conduit
e_calendar_conduitsdir=$(libdir)/gnome-pilot/conduits
e_calendar_conduits_LTLIBRARIES = libecalendar_conduit.la

libecalendar_conduit_la_SOURCES = 	\
	calendar-conduit.c		\
	calendar-conduit.h		\
	calendar-conduit-config.h

libecalendar_conduit_la_LIBADD = 					\
	$(top_builddir)/calendar/cal-client/libcal-client-static.la	\
	$(top_builddir)/calendar/cal-util/libcal-util-static.la		\
	$(top_builddir)/libversit/libversit.la				\
	$(top_builddir)/libical/src/libical/libical-static.la		\
	$(top_builddir)/e-util/libeconduit-static.la	 		\
	$(BONOBO_VFS_GNOME_LIBS)					\
	$(PISOCK_LIBDIR) $(PISOCK_LIBS) 				\
	$(GNOME_LIBDIR) 						\
	$(GNOME_LIBS)

e-calendar.conduit: e-calendar.conduit.in Makefile
	sed -e 's^\@prefix\@^$(prefix)^g' < $(srcdir)/e-calendar.conduit.in > e-calendar.conduit.tmp \
	&& mv e-calendar.conduit.tmp e-calendar.conduit

ccenterdir = $(datadir)/control-center
ccenterPalmPilotdir = $(ccenterdir)/Peripherals
ccenterConduitsdir = $(ccenterPalmPilotdir)/Conduits
ccenterConduits_DATA = e-calendar-conduit-control-applet.desktop 

panelConduitsdir = $(datadir)/gnome/apps/Settings/Peripherals/Conduits
panelConduits_DATA = $(ccenterConduits_DATA)

Conduitdir = $(datadir)/gnome-pilot/conduits/
Conduit_DATA = e-calendar.conduit

EXTRA_DIST = \
	e-calendar.conduit.in \
        $(ccenterConduits_DATA)

install-data-local:
	$(mkinstalldirs) $(ccenterConduitsdir)
	$(mkinstalldirs) $(Conduitdir)
