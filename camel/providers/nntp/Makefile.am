## Process this file with automake to produce Makefile.in

SUBDIRS = 

libcamelnntpincludedir = $(includedir)/camel

providerdir = $(pkglibdir)/camel-providers/$(VERSION)

provider_LTLIBRARIES = libcamelnntp.la
provider_DATA = libcamelnntp.urls

INCLUDES = -I.. -I$(srcdir)/.. -I$(includedir)	\
	-I$(top_srcdir)/intl			\
	$(GTK_INCLUDEDIR) -I$(top_srcdir)/camel	\
	-I$(srcdir)/../mbox			\
	-DG_LOG_DOMAIN=\"camel-nntp-provider\"

libcamelnntp_la_SOURCES = 			\
	camel-nntp-folder.c			\
	camel-nntp-provider.c			\
	camel-nntp-store.c			\
	camel-nntp-summary.c			\
	camel-nntp-utils.c

libcamelnntpinclude_HEADERS =			\
	camel-nntp-folder.h			\
	camel-nntp-store.h			\
	camel-nntp-summary.h


libcamelnntp_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir) 

EXTRA_DIST = libcamelnntp.urls
