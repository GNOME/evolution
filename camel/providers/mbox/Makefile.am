## Process this file with automake to produce Makefile.in

SUBDIRS = 

libcamelmboxincludedir = $(includedir)/camel


providerdir = $(pkglibdir)/camel-providers/$(VERSION)

provider_LTLIBRARIES = libcamelmbox.la
provider_DATA = libcamelmbox.urls

INCLUDES = -I.. \
	-I$(srcdir)/..				\
	-I$(top_srcdir)/camel			\
	-I$(top_srcdir)/intl			\
	-I$(top_srcdir)/libibex			\
	-I$(top_srcdir)/e-util			\
	-I$(top_srcdir)				\
	-I$(includedir)				\
	$(GTK_INCLUDEDIR)			\
	-DG_LOG_DOMAIN=\"camel-mbox-provider\"	

libcamelmbox_la_SOURCES = 			\
	camel-mbox-folder.c			\
	camel-mbox-provider.c			\
	camel-mbox-store.c			\
	camel-mbox-summary.c

libcamelmboxinclude_HEADERS =			\
	camel-mbox-folder.h			\
	camel-mbox-store.h			\
	camel-mbox-summary.h

libcamelmbox_la_LDFLAGS = -version-info 0:0:0

libcamelmbox_la_LIBADD = $(top_builddir)/e-util/libeutil.la $(top_builddir)/libibex/libibex.la $(UNICODE_LIBS)
#libcamelmbox_la_LIBADD = $(top_builddir)/libibex/libibex.la $(UNICODE_LIBS)

EXTRA_DIST = libcamelmbox.urls

