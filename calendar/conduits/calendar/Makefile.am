AM_CPPFLAGS = 					\
	-I$(top_srcdir)/e-util			\
	-I$(top_builddir)/e-util		\
	-I$(top_srcdir)/widgets/misc		\
	-I$(top_builddir)/widgets/misc		\
	-I$(top_srcdir)/calendar/conduits/common		\
	-I$(top_builddir)/calendar/conduits/common		\
	$(EVOLUTION_CALENDAR_CONDUIT_CFLAGS)

# Calendar Conduit
privconduit_LTLIBRARIES = libecalendar_conduit.la

libecalendar_conduit_la_SOURCES = 		\
	calendar-conduit.c

libecalendar_conduit_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

libecalendar_conduit_la_LIBADD = 					\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/e-util/libeconduit.la				\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/calendar/conduits/common/libecalendar_common_conduit.la	\
	$(EVOLUTION_CALENDAR_CONDUIT_LIBS)

e-calendar.conduit: e-calendar.conduit.in
	sed -e 's^\@privconduitdir\@^$(privconduitdir)^g'		\
	    -e 's^\@datadir\@^$(datadir)^g' 				\
	    -e 's^\@BASE_VERSION\@^$(BASE_VERSION)^g' 			\
	    $< > $@

conduitdir = $(datadir)/gnome-pilot/conduits/
conduit_DATA = e-calendar.conduit

BUILT_SOURCES = $(conduit_DATA)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	e-calendar.conduit.in

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
