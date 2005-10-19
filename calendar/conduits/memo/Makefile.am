INCLUDES = 					\
	-I$(top_srcdir)/e-util			\
	-I$(top_builddir)/e-util		\
	-I$(top_srcdir)/widgets/misc		\
	-I$(top_builddir)/widgets/misc		\
	$(EVOLUTION_CALENDAR_CONDUIT_CFLAGS)

# Memo Conduit
privconduit_LTLIBRARIES = libememo_conduit.la

libememo_conduit_la_SOURCES = 	\
	memo-conduit.c

libememo_conduit_la_LDFLAGS = -module -avoid-version
libememo_conduit_la_LIBADD = 						\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/e-util/libeconduit.la		 		\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		 	\
	$(EVOLUTION_CALENDAR_CONDUIT_LIBS)

e-memo-$(BASE_VERSION).conduit: e-memo.conduit.in
	sed -e 's^\@privconduitdir\@^$(privconduitdir)^g' 			\
	    -e 's^\@datadir\@^$(datadir)^g' 				\
	    -e 's^\@BASE_VERSION\@^$(BASE_VERSION)^g' 			\
	    $< > $@

conduitdir = $(datadir)/gnome-pilot/conduits/
conduit_DATA = e-memo-$(BASE_VERSION).conduit

BUILT_SOURCES = $(conduit_DATA)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	e-memo.conduit.in

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
