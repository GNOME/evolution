## Process this file with automake to produce Makefile.in

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
	$(KRB4_CFLAGS)				\
	-DG_LOG_DOMAIN=\"camel-imap-provider\"	

libcamelimap_la_SOURCES = 			\
	camel-imap-auth.c			\
	camel-imap-command.c			\
	camel-imap-folder.c			\
	camel-imap-provider.c			\
	camel-imap-search.c			\
	camel-imap-store.c			\
	camel-imap-summary.c			\
	camel-imap-utils.c

libcamelimapinclude_HEADERS =			\
	camel-imap-auth.h			\
	camel-imap-command.h			\
	camel-imap-folder.h			\
	camel-imap-search.h			\
	camel-imap-store.h			\
	camel-imap-summary.h			\
	camel-imap-types.h			\
	camel-imap-utils.h

libcamelimap_la_LDFLAGS = $(KRB4_LDFLAGS) -version-info 0:0:0

noinst_HEADERS =				\
	camel-imap-private.h

EXTRA_DIST = libcamelimap.urls





