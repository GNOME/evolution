INCLUDES = 					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/calendar		\
	-I$(top_builddir)/calendar/cal-client	\
	-I$(top_srcdir)/libical/src/libical	\
	-I$(top_builddir)/libical/src/libical	\
	-I$(top_srcdir)/e-util			\
	-I$(top_builddir)/e-util		\
	$(EVOLUTION_CALENDAR_CONDUIT_CFLAGS)

# ToDo Conduit
e_todo_conduitsdir=$(libdir)/gnome-pilot/conduits
e_todo_conduits_LTLIBRARIES = libetodo_conduit.la

libetodo_conduit_la_SOURCES = 	\
	todo-conduit.c

libetodo_conduit_la_LDFLAGS = -module -avoid-version
libetodo_conduit_la_LIBADD = 						\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/calendar/cal-client/libcal-client-static.la	\
	$(top_builddir)/calendar/cal-util/libcal-util-static.la		\
	$(top_builddir)/libversit/libversit.la				\
	$(top_builddir)/libical/src/libical/libical-static.la		\
	$(top_builddir)/libwombat/libwombat-static.la	 		\
	$(top_builddir)/e-util/libeconduit-static.la	 		\
	$(EVOLUTION_CALENDAR_CONDUIT_LIBS)

e-todo.conduit: e-todo.conduit.in Makefile
	sed -e 's^\@prefix\@^$(prefix)^g' 				\
	    -e 's^\@datadir\@^$(datadir)^g' 				\
	    < $(srcdir)/e-todo.conduit.in > e-todo.conduit.tmp 		\
	&& mv e-todo.conduit.tmp e-todo.conduit

Conduitdir = $(datadir)/gnome-pilot/conduits/
Conduit_DATA = e-todo.conduit

EXTRA_DIST = \
	e-todo.conduit.in

install-data-local:
	$(mkinstalldirs) $(Conduitdir)
