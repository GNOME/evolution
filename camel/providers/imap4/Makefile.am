## Process this file with automake to produce Makefile.in

libcamelimap4includedir = $(privincludedir)/camel

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
	camel-imap-command.c			\
	camel-imap-command.h			\
	camel-imap-engine.c			\
	camel-imap-engine.h			\
	camel-imap-specials.c			\
	camel-imap-specials.h			\
	camel-imap-stream.c			\
	camel-imap-stream.h			\
	camel-imap-summary.c			\
	camel-imap-summary.h			\
	camel-imap-utils.c			\
	camel-imap-utils.h

libcamelimap4_la_LDFLAGS = -avoid-version -module

EXTRA_DIST = libcamelimap4.urls
