## Process this file with automake to produce Makefile.in

libcamelpop3includedir = $(includedir)/camel

providerdir = $(pkglibdir)/camel-providers/$(VERSION)

provider_LTLIBRARIES = libcamelpop3.la
provider_DATA = libcamelpop3.urls

INCLUDES =					\
	-I..					\
	-I$(srcdir)/..				\
	-I$(srcdir)/../../..			\
        -I$(includedir)				\
	-I$(top_srcdir)/intl			\
	$(GTK_INCLUDEDIR)			\
	-I$(top_srcdir)/camel			\
	$(KRB4_CFLAGS)				\
	-DG_LOG_DOMAIN=\"camel-pop3-provider\"

libcamelpop3_la_SOURCES = 			\
	camel-pop3-folder.c			\
	camel-pop3-provider.c			\
	camel-pop3-store.c

libcamelpop3include_HEADERS =			\
	camel-pop3-folder.h			\
	camel-pop3-store.h


libcamelpop3_la_LDFLAGS = $(KRB4_LDFLAGS) -version-info 0:0:0

EXTRA_DIST = libcamelpop3.urls
