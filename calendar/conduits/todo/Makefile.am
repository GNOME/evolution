#
#
#

INCLUDES = 				\
	-I$(top_srcdir)/calendar	\
	$(GNOME_PILOT_CFLAGS)

file_conduitsdir=$(libdir)/gnome-pilot/conduits

bin_PROGRAMS = todo-conduit-control-applet

todo_conduit_control_applet_SOURCES = todo-conduit-control-applet.c

todo_conduit_control_applet_LDADD = \
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


# todo_conduit
todo_conduitsdir=$(libdir)/gnome-pilot/conduits
todo_conduits_LTLIBRARIES = libtodo_conduit.la

libtodo_conduit_la_SOURCES = \
	todo-conduit.c

# todo-conduit.c: my_gnomecal_idl

todo.conduit: todo.conduit.in Makefile
	sed -e 's^\@prefix\@^$(prefix)^g' < $(srcdir)/todo.conduit.in > todo.conduit.tmp \
	&& mv todo.conduit.tmp todo.conduit

#
# conduit
#

libtodo_conduit_la_LDFLAGS = \
	-rpath $(libdir)

libtodo_conduit_la_LIBADD = 					\
	$(top_builddir)/calendar/cal-client/libcal-client-static.la	\
	$(top_builddir)/calendar/cal-util/libcal-util-static.la		\
	$(top_builddir)/libversit/libversit.la				\
	$(top_builddir)/libical/src/libical/libical-static.la		\
	$(BONOBO_VFS_GNOME_LIBS)					\
	$(PISOCK_LIBDIR) $(PISOCK_LIBS) 				\
	$(GNOME_LIBDIR) 						\
	$(GNOME_LIBS)

ccenterdir = $(datadir)/control-center
ccenterPalmPilotdir = $(ccenterdir)/Peripherals
ccenterConduitsdir = $(ccenterPalmPilotdir)/Conduits
ccenterConduits_DATA = todo-conduit-control-applet.desktop 

panelConduitsdir = $(datadir)/gnome/apps/Settings/Peripherals/Conduits
panelConduits_DATA = $(ccenterConduits_DATA)

Conduitdir = $(datadir)/gnome-pilot/conduits/
Conduit_DATA = todo.conduit

EXTRA_DIST = \
	todo.conduit.in \
        $(ccenterConduits_DATA)

install-data-local:
	$(mkinstalldirs) $(ccenterConduitsdir)
	$(mkinstalldirs) $(Conduitdir)
