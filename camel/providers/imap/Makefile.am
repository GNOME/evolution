## Process this file with automake to produce Makefile.in

SUBDIRS = 

libcamelimapincludedir = $(includedir)/camel


providerdir = $(pkglibdir)/camel-providers/$(VERSION)

provider_LTLIBRARIES = libcamelimap.la
provider_DATA = libcamelimap.urls

INCLUDES = -I.. \
	-I$(srcdir)/..				\
	-I$(top_srcdir)/camel			\
	-I$(top_srcdir)/intl			\
	-I$(top_srcdir)/libibex			\
	-I$(top_srcdir)/e-util			\
	-I$(top_srcdir)				\
	-I$(includedir)				\
	$(GTK_INCLUDEDIR)			\
	-DG_LOG_DOMAIN=\"camel-imap-provider\"	

libcamelimap_la_SOURCES = 			\
	camel-imap-folder.c			\
	camel-imap-provider.c			\
	camel-imap-store.c			\
	camel-imap-stream.c			\
	camel-imap-summary.c

libcamelimapinclude_HEADERS =			\
	camel-imap-folder.h			\
	camel-imap-store.h			\
	camel-imap-stream.h			\
	camel-imap-summary.h

libcamelimap_la_LDFLAGS = -version-info 0:0:0

libcamelimap_la_LIBADD = $(top_builddir)/e-util/libeutil.la $(top_builddir)/libibex/libibex.la $(UNICODE_LIBS)
#libcamelimap_la_LIBADD = $(top_builddir)/libibex/libibex.la $(UNICODE_LIBS)

EXTRA_DIST = libcamelimap.urls





