## Process this file with automake to produce Makefile.in

SUBDIRS = 

libcamelmboxincludedir = $(includedir)/camel


providerdir = $(pkglibdir)/camel-providers/$(VERSION)

provider_LTLIBRARIES = libcamelmbox.la

INCLUDES = -I.. -I$(srcdir)/.. -I$(includedir)	\
	-I$(top_srcdir)/intl			\
	$(GTK_INCLUDEDIR) -I$(top_srcdir)/camel	\
	-I$(top_srcdir)/libibex

libcamelmbox_la_SOURCES = 			\
	camel-mbox-folder.c			\
	camel-mbox-parser.c			\
	camel-mbox-provider.c			\
	camel-mbox-store.c			\
	camel-mbox-summary.c			\
	camel-mbox-search.c			\
	camel-mbox-utils.c

libcamelmboxinclude_HEADERS =			\
	camel-mbox-folder.h			\
	camel-mbox-parser.h			\
	camel-mbox-store.h			\
	camel-mbox-summary.h			\
	camel-mbox-search.h			\
	camel-mbox-utils.h


libcamelmbox_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir) 

libcamelmbox_la_LIBADD = -L$(top_builddir)/libibex -libex
#libcamelmbox_la_LIBADD = $(top_builddir)/libibex/libibex.la $(UNICODE_LIBS)
#libcamelmbox_la_LIBADD =   $(UNICODE_LIBS)

EXTRA_DIST = 
