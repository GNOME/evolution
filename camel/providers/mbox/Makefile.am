## Process this file with automake to produce Makefile.in

SUBDIRS = 

libcamelmboxincludedir = $(includedir)/camel


providerdir = $(pkglibdir)/camel-providers/$(VERSION)

provider_LTLIBRARIES = libcamelmbox.la

INCLUDES = -I.. \
	-I$(srcdir)/..				\
	-I$(top_srcdir)/camel			\
	-I$(top_srcdir)/intl			\
	-I$(top_srcdir)/libibex			\
	-I$(top_srcdir)/e-util			\
	-I$(top_srcdir)				\
	-I$(includedir)				\
	$(GTK_INCLUDEDIR)

libcamelmbox_la_SOURCES = 			\
	camel-mbox-folder.c			\
	camel-mbox-parser.c			\
	camel-mbox-provider.c			\
	camel-mbox-store.c			\
	camel-mbox-search.c			\
	camel-mbox-summary.c			\
	camel-mbox-utils.c

libcamelmboxinclude_HEADERS =			\
	camel-mbox-folder.h			\
	camel-mbox-parser.h			\
	camel-mbox-store.h			\
	camel-mbox-search.h			\
	camel-mbox-summary.h			\
	camel-mbox-utils.h

libcamelmbox_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir) 

libcamelmbox_la_LIBADD = $(top_builddir)/e-util/libeutil.la $(top_builddir)/libibex/libibex.la $(UNICODE_LIBS)
#libcamelmbox_la_LIBADD = $(top_builddir)/libibex/libibex.la $(UNICODE_LIBS)

EXTRA_DIST = 

