INCLUDES = 					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/addressbook		\
	-I$(top_srcdir)/addressbook/backend	\
	-I$(top_builddir)/e-util		\
	-I$(top_srcdir)/e-util			\
	$(BONOBO_GNOME_CFLAGS)			\
	$(PISOCK_INCLUDEDIR)			\
	$(GNOME_PILOT_CFLAGS)

# Address Capplet
bin_PROGRAMS = e-address-conduit-control-applet

e_address_conduit_control_applet_SOURCES = address-conduit-control-applet.c

e_address_conduit_control_applet_LDADD = 	\
		$(CAPPLET_LIBS)  		\
		$(GNOME_LIBDIR) 		\
		$(GNOME_PILOT_LIBS) 		\
		$(PISOCK_LIBDIR) $(PISOCK_LIBS) \
		$(GNOME_CAPPLET_LIBS) 		\
		$(BONOBO_GNOME_LIBS) 		\
		$(GNOME_XML_LIB) 		\
	        $(GNOMEUI_LIBS) 		\
                $(INTLLIBS)


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
	$(PISOCK_LIBDIR) $(PISOCK_LIBS) 				\
	$(BONOBO_GNOME_LIBS)						\
	$(EXTRA_GNOME_LIBS)						\
	$(GNOME_LIBDIR) 						\
	$(GNOME_LIBS)

e-address.conduit: e-address.conduit.in Makefile
	sed -e 's^\@prefix\@^$(prefix)^g' < $(srcdir)/e-address.conduit.in > e-address.conduit.tmp \
	&& mv e-address.conduit.tmp e-address.conduit


ccenterdir = $(datadir)/control-center
ccenterPalmPilotdir = $(ccenterdir)/Peripherals
ccenterConduitsdir = $(ccenterPalmPilotdir)/Conduits
ccenterConduits_in_files = e-address-conduit-control-applet.desktop.in 
ccenterConduits_DATA = $(ccenterConduits_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

panelConduitsdir = $(datadir)/gnome/apps/Settings/Peripherals/Conduits
panelConduits_DATA = $(ccenterConduits_DATA)

Conduitdir = $(datadir)/gnome-pilot/conduits/
Conduit_DATA = e-address.conduit

EXTRA_DIST = \
	e-address.conduit.in \
        $(ccenterConduits_in_files)

install-data-local:
	$(mkinstalldirs) $(ccenterConduitsdir)
	$(mkinstalldirs) $(Conduitdir)

