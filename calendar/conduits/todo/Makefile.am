INCLUDES = 					\
	-I$(top_srcdir)/e-util			\
	-I$(top_builddir)/e-util		\
	-I$(top_srcdir)/widgets/misc		\
	-I$(top_builddir)/widgets/misc		\
	$(EVOLUTION_CALENDAR_CONDUIT_CFLAGS)

# ToDo Conduit
privconduit_LTLIBRARIES = libetodo_conduit.la

libetodo_conduit_la_SOURCES = 	\
	todo-conduit.c

libetodo_conduit_la_LDFLAGS = -module -avoid-version
libetodo_conduit_la_LIBADD = 						\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/e-util/libeconduit.la		 		\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		 	\
	$(EVOLUTION_CALENDAR_CONDUIT_LIBS)

e-todo-$(BASE_VERSION).conduit: e-todo.conduit.in
	sed -e 's^\@privconduitdir\@^$(privconduitdir)^g' 			\
	    -e 's^\@datadir\@^$(datadir)^g' 				\
	    -e 's^\@BASE_VERSION\@^$(BASE_VERSION)^g' 			\
	    $< > $@

conduitdir = $(datadir)/gnome-pilot/conduits/
conduit_DATA = e-todo-$(BASE_VERSION).conduit

BUILT_SOURCES = $(conduit_DATA)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	e-todo.conduit.in

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)