lib_LTLIBRARIES = libevolution-services.la

INCLUDES = \
	-I$(top_srcdir)					\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/executive-summary		\
	-I$(top_builddir)/executive-summary		\
	$(EXTRA_GNOME_CFLAGS)				\
	$(GNOME_VFS_CFLAGS)				\
	$(UNICODE_CFLAGS)				\
	$(GTKHTML_CFLAGS)				\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"            \
	-DG_LOG_DOMAIN=\"evolution-services\"

IDL_GENERATED = \
	Executive-Summary.h		\
	Executive-Summary-common.c	\
	Executive-Summary-skels.c	\
	Executive-Summary-stubs.c

Executive-Summary-impl.o: Executive-Summary.h

IDLS = \
	$(srcdir)/../idl/Executive-Summary.idl		\
	$(srcdir)/../idl/Summary.idl			\
	$(srcdir)/../idl/SummaryComponent.idl

$(IDL_GENERATED): $(IDLS)
	$(ORBIT_IDL) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl` \
		-I$(srcdir) $(srcdir)/../idl/Executive-Summary.idl

libevolution_services_la_SOURCES =		\
	$(IDL_GENERATED)		\
	executive-summary.c		\
	executive-summary.h		\
	executive-summary-client.c	\
	executive-summary-client.h	\
	executive-summary-component.c	\
	executive-summary-component.h	\
	executive-summary-component-client.c	\
	executive-summary-component-client.h	\
	executive-summary-component-view.c	\
	executive-summary-component-view.h

libevolution_services_la_LIBADD = 					\
	$(top_builddir)/e-util/libeutil.la		\
	$(BONOBO_VFS_GNOME_LIBS)			\
	$(EXTRA_GNOME_LIBS)				\
	$(GTKHTML_LIBS)					\
	$(UNICODE_LIBS)

