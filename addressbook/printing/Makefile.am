ecpsdir    = $(privdatadir)/ecps
ecps_DATA  =				\
	smallbook.ecps			\
	medbook.ecps			\
	phonelist.ecps

libecontactprint_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)				\
	-DG_LOG_DOMAIN=\"addressbook-printing\"	\
	-I$(top_srcdir)/addressbook		\
	-I$(top_srcdir)				\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"	\
	-DEVOLUTION_ECPSDIR=\""$(ecpsdir)"\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)		\
	$(GNOME_PLATFORM_CFLAGS)		\
	$(CODE_COVERAGE_CFLAGS)			\
	$(NULL)

noinst_LTLIBRARIES = libecontactprint.la

libecontactprint_la_LDFLAGS = $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

libecontactprint_la_SOURCES = 			\
	e-contact-print-types.h			\
	e-contact-print.c			\
	e-contact-print.h

libecontactprint_la_LIBADD =					\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/addressbook/util/libeabutil.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(NULL)

EXTRA_DIST = 		\
	$(ecps_DATA)

-include $(top_srcdir)/git.mk
