INCLUDES = 					\
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
	$(top_builddir)/e-util/libeconduit.la	 			\
	$(EVOLUTION_CALENDAR_CONDUIT_LIBS)

e-calendar-$(BASE_VERSION).conduit: e-calendar.conduit.in
	sed -e 's^\@prefix\@^$(prefix)^g' 				\
	    -e 's^\@datadir\@^$(datadir)^g' 				\
	    -e 's^\@BASE_VERSION\@^$(BASE_VERSION)^g' 			\
	    $< > $@

conduitdir = $(datadir)/gnome-pilot/conduits/
conduit_DATA = e-calendar-$(BASE_VERSION).conduit

BUILT_SOURCES = $(conduit_DATA)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	e-calendar.conduit.in

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)