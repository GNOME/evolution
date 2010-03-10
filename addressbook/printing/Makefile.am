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
	$(GNOME_PLATFORM_CFLAGS)		\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

noinst_LTLIBRARIES = libecontactprint.la

libecontactprint_la_SOURCES = 			\
	e-contact-print-types.h			\
	e-contact-print.c			\
	e-contact-print.h

libecontactprint_la_LIBADD =					\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(GNOME_PLATFORM_LIBS)

noinst_PROGRAMS = contact-print-test

contact_print_test_CPPFLAGS = \
	$(AM_CPPFLAGS)				\
	-DG_LOG_DOMAIN=\"addressbook-printing\"	\
	-I$(top_srcdir)/addressbook		\
	-I$(top_srcdir)				\
	-DEVOLUTION_ECPSDIR=\""$(ecpsdir)"\"	\
	$(GNOME_PLATFORM_CFLAGS)		\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

contact_print_test_SOURCES = test-print.c

contact_print_test_LDADD =	\
	libecontactprint.la					\
	$(top_builddir)/addressbook/util/libeabutil.la		\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(EVOLUTION_ADDRESSBOOK_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

EXTRA_DIST = 		\
	$(ecps_DATA)

-include $(top_srcdir)/git.mk
