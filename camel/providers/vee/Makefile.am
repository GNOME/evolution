## Process this file with automake to produce Makefile.in

SUBDIRS = 

libcamelveeincludedir = $(includedir)/camel


providerdir = $(pkglibdir)/camel-providers/$(VERSION)

provider_LTLIBRARIES = libcamelvee.la
provider_DATA = libcamelvee.urls

INCLUDES = -I.. \
	-I$(srcdir)/..				\
	-I$(top_srcdir)/camel			\
	-I$(top_srcdir)/intl			\
	-I$(top_srcdir)/libibex			\
	-I$(top_srcdir)/e-util			\
	-I$(top_srcdir)				\
	-I$(includedir)				\
	$(GTK_INCLUDEDIR)			\
	-DG_LOG_DOMAIN=\"camel-vee-provider\"	

libcamelvee_la_SOURCES = 			\
	camel-vee-folder.c			\
	camel-vee-provider.c			\
	camel-vee-store.c

libcamelveeinclude_HEADERS =			\
	camel-vee-folder.h			\
	camel-vee-store.h

libcamelvee_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir) 

libcamelvee_la_LIBADD = $(top_builddir)/e-util/libeutil.la $(top_builddir)/libibex/libibex.la $(UNICODE_LIBS)
#libcamelvee_la_LIBADD = $(top_builddir)/libibex/libibex.la $(UNICODE_LIBS)

EXTRA_DIST = libcamelvee.urls

