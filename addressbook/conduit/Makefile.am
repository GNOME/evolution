#
#
#

INCLUDES = 				\
	-I$(top_srcdir)/calendar	\
	$(GNOME_PILOT_CFLAGS)

file_conduitsdir=$(libdir)/gnome-pilot/conduits

bin_PROGRAMS = address-conduit-control-applet

address_conduit_control_applet_SOURCES = address-conduit-control-applet.c

address_conduit_control_applet_LDADD = \
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


# address_conduit
address_conduitsdir=$(libdir)/gnome-pilot/conduits
address_conduits_LTLIBRARIES = libaddress_conduit.la

libaddress_conduit_la_SOURCES = \
	address-conduit.c

# address-conduit.c: my_gnomecal_idl

address.conduit: address.conduit.in Makefile
	sed -e 's^\@prefix\@^$(prefix)^g' < $(srcdir)/address.conduit.in > address.conduit.tmp \
	&& mv address.conduit.tmp address.conduit

#
# conduit
#

libaddress_conduit_la_LDFLAGS = \
	-rpath $(libdir)

libaddress_conduit_la_LIBADD = 					\
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
ccenterConduits_DATA = address-conduit-control-applet.desktop 

panelConduitsdir = $(datadir)/gnome/apps/Settings/Peripherals/Conduits
panelConduits_DATA = $(ccenterConduits_DATA)

Conduitdir = $(datadir)/gnome-pilot/conduits/
Conduit_DATA = address.conduit

EXTRA_DIST = \
	address.conduit.in \
        $(ccenterConduits_DATA)

install-data-local:
	$(mkinstalldirs) $(ccenterConduitsdir)
	$(mkinstalldirs) $(Conduitdir)
