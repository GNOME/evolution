#
#
#

INCLUDES = 				\
	-I$(top_srcdir)/calendar	\
	$(GNOME_PILOT_CFLAGS)

file_conduitsdir=$(libdir)/gnome-pilot/conduits

bin_PROGRAMS = calendar-conduit-control-applet

calendar_conduit_control_applet_SOURCES = calendar-conduit-control-applet.c

calendar_conduit_control_applet_LDADD = \
		$(CAPPLET_LIBS)  \
		$(GNOME_LIBDIR) \
		-lgpilotdcm \
		-lgpilotd \
		-lgpilotdconduit \
		$(GNOME_CAPPLET_LIBS) \
		$(BONOBO_VFS_GNOME_LIBS) \
		$(GNOME_XML_LIB) \
	        $(GNOMEUI_LIBS) \
		$(PISOCK_LIBDIR) $(PISOCK_LIBS) \
                $(INTLLIBS)


#		$(top_builddir)/libgpilotdCM/libgpilotdcm.la \
#		$(top_builddir)/gpilotd/libgpilotd.la \
#		$(top_builddir)/gpilotd/libgpilotdconduit.la \


if HAVE_GNOME_PILOT
# calendar_conduit
calendar_conduitsdir=$(libdir)/gnome-pilot/conduits
calendar_conduits_LTLIBRARIES = libcalendar_conduit.la

libcalendar_conduit_la_SOURCES = \
	calendar-conduit.c

# calendar-conduit.c: my_gnomecal_idl

calendar.conduit: calendar.conduit.in Makefile
	sed -e 's^\@prefix\@^$(prefix)^g' < $(srcdir)/calendar.conduit.in > calendar.conduit.tmp \
	&& mv calendar.conduit.tmp calendar.conduit
endif 


libcalendar_conduit_la_LDFLAGS = \
	-rpath $(libdir)

libcalendar_conduit_la_LIBADD = 				\
	$(BONOBO_VFS_GNOME_LIBS)				\
	$(top_builddir)/calendar/cal-client/libcal-client.a	\
	$(PISOCK_LIBDIR) $(PISOCK_LIBS) 			\
	$(GNOME_LIBDIR) 					\
	$(GNOME_LIBS)


#	$(top_builddir)/calendar/cal-util/libcal-util.la	\
#	$(top_builddir)/libversit/libversit.la			\
#	$(top_builddir)/libical/src/libical/libical.la 		\


ccenterdir = $(datadir)/control-center
ccenterPalmPilotdir = $(ccenterdir)/Peripherals
ccenterConduitsdir = $(ccenterPalmPilotdir)/Conduits
ccenterConduits_DATA = calendar-conduit-control-applet.desktop 

panelConduitsdir = $(datadir)/gnome/apps/Settings/Peripherals/Conduits
panelConduits_DATA = $(ccenterConduits_DATA)

Conduitdir = $(datadir)/gnome-pilot/conduits/
Conduit_DATA = calendar.conduit

EXTRA_DIST = \
	$(Conduit_DATA) \
        $(ccenterConduits_DATA)

install-data-local:
	$(mkinstalldirs) $(ccenterConduitsdir)
	$(mkinstalldirs) $(Conduitdir)
