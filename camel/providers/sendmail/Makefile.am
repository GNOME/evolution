## Process this file with automake to produce Makefile.in

libcamelsendmailincludedir = $(includedir)/camel


providerdir = $(pkglibdir)/camel-providers/$(VERSION)

provider_LTLIBRARIES = libcamelsendmail.la
provider_DATA = libcamelsendmail.urls

INCLUDES =					\
	-I..					\
	-I$(srcdir)/..				\
	-I$(srcdir)/../../..			\
	-I$(includedir)				\
	-I$(top_srcdir)/intl			\
	$(GTK_INCLUDEDIR) -I$(top_srcdir)/camel \
        -DG_LOG_DOMAIN=\"camel-sendmail-provider\"

libcamelsendmail_la_SOURCES = 			\
	camel-sendmail-provider.c		\
	camel-sendmail-transport.c

libcamelsendmailinclude_HEADERS =		\
	camel-sendmail-transport.h

libcamelsendmail_la_LDFLAGS = -version-info 0:0:0

EXTRA_DIST = libcamelsendmail.urls
