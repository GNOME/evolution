## Process this file with automake to produce Makefile.in

libcamellocalincludedir = $(privincludedir)/camel

camel_provider_LTLIBRARIES = libcamellocal.la
camel_provider_DATA = libcamellocal.urls

INCLUDES = -I.. \
	-I$(srcdir)/..				\
	-I$(top_srcdir)/camel			\
	-I$(top_srcdir)/intl			\
	-I$(top_srcdir)/e-util			\
	-I$(top_srcdir)				\
	$(CAMEL_CFLAGS)				\
	$(GNOME_INCLUDEDIR)			\
	$(GTK_INCLUDEDIR)			\
	-DG_LOG_DOMAIN=\"camel-local-provider\"

libcamellocal_la_SOURCES = 			\
	camel-local-folder.c			\
	camel-local-store.c			\
	camel-local-summary.c			\
	camel-local-provider.c			\
	camel-mh-folder.c			\
	camel-mh-store.c			\
	camel-mh-summary.c			\
	camel-mbox-folder.c			\
	camel-mbox-store.c			\
	camel-mbox-summary.c			\
	camel-maildir-folder.c			\
	camel-maildir-store.c			\
	camel-maildir-summary.c			\
	camel-spool-folder.c			\
	camel-spool-store.c			\
	camel-spool-summary.c

libcamellocalinclude_HEADERS =			\
	camel-local-folder.h			\
	camel-local-store.h			\
	camel-local-summary.h			\
	camel-mh-folder.h			\
	camel-mh-store.h			\
	camel-mh-summary.h			\
	camel-mbox-folder.h			\
	camel-mbox-store.h			\
	camel-mbox-summary.h			\
	camel-maildir-folder.h			\
	camel-maildir-store.h			\
	camel-maildir-summary.h			\
	camel-spool-folder.h			\
	camel-spool-store.h			\
	camel-spool-summary.h

noinst_HEADERS =				\
	camel-local-private.h

libcamellocal_la_LDFLAGS = -avoid-version -module

libcamellocal_la_LIBADD = $(top_builddir)/e-util/libeutil.la

EXTRA_DIST = libcamellocal.urls
