INCLUDES = 					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/calendar		\
	-I$(top_srcdir)/libical/src/libical	\
	-I$(top_builddir)/libical/src/libical	\
	-I$(top_builddir)/e-util		\
	$(BONOBO_GNOME_CFLAGS)			\
	$(PISOCK_INCLUDEDIR)			\
	$(GNOME_PILOT_CFLAGS)

# ToDo Capplet
bin_PROGRAMS = e-todo-conduit-control-applet

e_todo_conduit_control_applet_SOURCES = todo-conduit-control-applet.c

e_todo_conduit_control_applet_LDADD = 		\
		$(CAPPLET_LIBS)  		\
		$(GNOME_LIBDIR) 		\
		$(GNOME_PILOT_LIBS) 		\
		$(PISOCK_LIBS) 			\
		$(GNOME_CAPPLET_LIBS) 		\
		$(BONOBO_GNOME_LIBS) 		\
		$(GNOME_XML_LIB) 		\
	        $(GNOMEUI_LIBS) 		\
                $(INTLLIBS)

# ToDo Conduit
e_todo_conduitsdir=$(libdir)/gnome-pilot/conduits
e_todo_conduits_LTLIBRARIES = libetodo_conduit.la

libetodo_conduit_la_SOURCES = 	\
	todo-conduit.c 		\
	todo-conduit.h		\
	todo-conduit-config.h 

libetodo_conduit_la_LDFLAGS = -module -avoid-version
libetodo_conduit_la_LIBADD = 						\
	$(top_builddir)/calendar/cal-client/libcal-client-static.la	\
	$(top_builddir)/calendar/cal-util/libcal-util-static.la		\
	$(top_builddir)/libversit/libversit.la				\
	$(top_builddir)/libical/src/libical/libical-static.la		\
	$(top_builddir)/e-util/libeconduit-static.la	 		\
	$(BONOBO_GNOME_LIBS)						\
	$(PISOCK_LIBS) 							\
	$(UNICODE_LIBS)							\
	$(GNOME_LIBDIR) 						\
	$(GNOME_LIBS)

e-todo.conduit: e-todo.conduit.in Makefile
	sed -e 's^\@prefix\@^$(prefix)^g' 				\
	    -e 's^\@datadir\@^$(datadir)^g' 				\
	    < $(srcdir)/e-todo.conduit.in > e-todo.conduit.tmp 		\
	&& mv e-todo.conduit.tmp e-todo.conduit


ccenterdir = $(datadir)/control-center
ccenterPalmPilotdir = $(ccenterdir)/Peripherals
ccenterConduitsdir = $(ccenterPalmPilotdir)/Conduits
ccenterConduits_in_files = e-todo-conduit-control-applet.desktop.in 
ccenterConduits_DATA = $(ccenterConduits_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

panelConduitsdir = $(datadir)/gnome/apps/Settings/Peripherals/Conduits
panelConduits_DATA = $(ccenterConduits_DATA)

Conduitdir = $(datadir)/gnome-pilot/conduits/
Conduit_DATA = e-todo.conduit

EXTRA_DIST = \
	e-todo.conduit.in \
        $(ccenterConduits_in_files)

install-data-local:
	$(mkinstalldirs) $(ccenterConduitsdir)
	$(mkinstalldirs) $(Conduitdir)
