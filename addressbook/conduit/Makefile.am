AM_CPPFLAGS = 					\
	-I$(top_srcdir)/e-util			\
	-I$(top_builddir)/e-util		\
	-I$(top_srcdir)/widgets/misc		\
	-I$(top_builddir)/widgets/misc		\
	$(EVOLUTION_ADDRESSBOOK_CONDUIT_CFLAGS)

# Address Conduit
privconduit_LTLIBRARIES = libeaddress_conduit.la

libeaddress_conduit_la_SOURCES = 		\
	address-conduit.c

libeaddress_conduit_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

libeaddress_conduit_la_LIBADD = 					\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/e-util/libeconduit.la                           \
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(EVOLUTION_ADDRESSBOOK_CONDUIT_LIBS)

e-address.conduit: e-address.conduit.in
	sed -e 's^\@privconduitdir\@^$(privconduitdir)^g' 		\
	    -e 's^\@datadir\@^$(datadir)^g' 				\
	    -e 's^\@BASE_VERSION\@^$(BASE_VERSION)^g' 			\
	    $< > $@

conduitdir = $(datadir)/gnome-pilot/conduits/
conduit_DATA = e-address.conduit

BUILT_SOURCES = $(conduit_DATA)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	e-address.conduit.in

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
