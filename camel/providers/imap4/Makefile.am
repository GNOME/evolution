## Process this file with automake to produce Makefile.in

camel_provider_LTLIBRARIES = libcamelimap4.la
camel_provider_DATA = libcamelimap4.urls

INCLUDES = 					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/camel			\
	-I$(top_srcdir)/intl			\
	-I$(top_srcdir)/e-util			\
	-I$(top_srcdir)				\
	$(CAMEL_CFLAGS)				\
	$(GNOME_INCLUDEDIR)			\
	$(GTK_INCLUDEDIR)			\
	-DG_LOG_DOMAIN=\"camel-imap4-provider\"

libcamelimap4_la_SOURCES = 			\
	camel-imap4-command.c			\
	camel-imap4-command.h			\
	camel-imap4-engine.c			\
	camel-imap4-engine.h			\
	camel-imap4-folder.c			\
	camel-imap4-folder.h			\
	camel-imap4-provider.c			\
	camel-imap4-search.c			\
	camel-imap4-search.h			\
	camel-imap4-specials.c			\
	camel-imap4-specials.h			\
	camel-imap4-store.c			\
	camel-imap4-store.h			\
	camel-imap4-store-summary.c		\
	camel-imap4-store-summary.h		\
	camel-imap4-stream.c			\
	camel-imap4-stream.h			\
	camel-imap4-summary.c			\
	camel-imap4-summary.h			\
	camel-imap4-utils.c			\
	camel-imap4-utils.h

libcamelimap4_la_LDFLAGS = -avoid-version -module

EXTRA_DIST = libcamelimap4.urls
