INCLUDES = 					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/addressbook		\
	-I$(top_srcdir)/addressbook/backend	\
	-I$(top_builddir)/addressbook/backend	\
	-I$(top_srcdir)/e-util			\
	-I$(top_builddir)/e-util		\
	$(PISOCK_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CONDUIT_CFLAGS)

# Address Conduit
e_address_conduitsdir=$(libdir)/gnome-pilot/conduits
e_address_conduits_LTLIBRARIES = libeaddress_conduit.la

libeaddress_conduit_la_SOURCES = \
	address-conduit.c

libeaddress_conduit_la_LDFLAGS = -module -avoid-version
libeaddress_conduit_la_LIBADD = 					\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/addressbook/backend/ebook/libebook-static.la	\
	$(top_builddir)/e-util/ename/libename-static.la			\
	$(top_builddir)/libversit/libversit.la				\
	$(top_builddir)/e-util/libeconduit.la		 		\
	$(top_builddir)/camel/libcamel.la	 			\
	$(EVOLUTION_ADDRESSBOOK_CONDUIT_LIBS)

e-address.conduit: e-address.conduit.in Makefile
	sed -e 's^\@prefix\@^$(prefix)^g' 				\
	    -e 's^\@datadir\@^$(datadir)^g' 				\
	    < $(srcdir)/e-address.conduit.in > e-address.conduit.tmp 	\
	&& mv e-address.conduit.tmp e-address.conduit


Conduitdir = $(datadir)/gnome-pilot/conduits/
Conduit_DATA = e-address.conduit

EXTRA_DIST = \
	e-address.conduit.in

install-data-local:
	$(mkinstalldirs) $(Conduitdir)
