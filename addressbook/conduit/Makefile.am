INCLUDES = 					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/addressbook		\
	-I$(top_srcdir)/addressbook/backend	\
	-I$(top_builddir)/e-util		\
	-I$(top_srcdir)/e-util			\
	$(BONOBO_GNOME_CFLAGS)			\
	$(PISOCK_INCLUDEDIR)			\
	$(GNOME_PILOT_CFLAGS)

# Address Conduit
e_address_conduitsdir=$(libdir)/gnome-pilot/conduits
e_address_conduits_LTLIBRARIES = libeaddress_conduit.la

libeaddress_conduit_la_SOURCES = \
	address-conduit.c \
	address-conduit.h \
	address-conduit-config.h

libeaddress_conduit_la_LDFLAGS = -module -avoid-version
libeaddress_conduit_la_LIBADD = 					\
	$(top_builddir)/addressbook/backend/ebook/libebook-static.la	\
	$(top_builddir)/e-util/libeutil-static.la			\
	$(top_builddir)/e-util/ename/libename-static.la			\
	$(top_builddir)/libversit/libversit.la				\
	$(top_builddir)/e-util/libeconduit-static.la	 		\
	$(GNOME_PILOT_LIBS) 						\
	$(PISOCK_LIBS) 							\
	$(BONOBO_GNOME_LIBS)						\
	$(EXTRA_GNOME_LIBS)						\
	$(GNOME_LIBDIR) 						\
	$(GNOME_LIBS)

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
