## Process this file with automake to produce Makefile.in

libcamelmhincludedir = $(includedir)/camel


providerdir = $(pkglibdir)/camel-providers/$(VERSION)

provider_LTLIBRARIES = libcamelmh.la
provider_DATA = libcamelmh.urls

INCLUDES = -I.. \
	-I$(srcdir)/..				\
	-I$(top_srcdir)/camel			\
	-I$(top_srcdir)/intl			\
	-I$(top_srcdir)/libibex			\
	-I$(top_srcdir)/e-util			\
	-I$(top_srcdir)				\
	-I$(includedir)				\
	$(GTK_INCLUDEDIR)			\
	-DG_LOG_DOMAIN=\"camel-mh-provider\"	

libcamelmh_la_SOURCES = 			\
	camel-mh-folder.c			\
	camel-mh-provider.c			\
	camel-mh-store.c			\
	camel-mh-summary.c

libcamelmhinclude_HEADERS =			\
	camel-mh-folder.h			\
	camel-mh-store.h			\
	camel-mh-summary.h

libcamelmh_la_LDFLAGS = -version-info 0:0:0

libcamelmh_la_LIBADD = $(top_builddir)/e-util/libeutil.la $(top_builddir)/libibex/libibex.la $(UNICODE_LIBS)
#libcamelmh_la_LIBADD = $(top_builddir)/libibex/libibex.la $(UNICODE_LIBS)

EXTRA_DIST = libcamelmh.urls

