## Process this file with automake to produce Makefile.in

libcamelpop3includedir = $(privincludedir)/camel

camel_provider_LTLIBRARIES = libcamelpop3.la
camel_provider_DATA = libcamelpop3.urls

INCLUDES =					\
	-I..					\
	-I$(srcdir)/..				\
	-I$(srcdir)/../../..			\
	-I$(top_srcdir)/intl			\
	-I$(top_srcdir)/camel			\
	-I$(top_srcdir)/e-util			\
	$(CAMEL_CFLAGS)				\
	$(GNOME_INCLUDEDIR)			\
	$(GTK_INCLUDEDIR)			\
	-DG_LOG_DOMAIN=\"camel-pop3-provider\"

libcamelpop3_la_SOURCES = 			\
	camel-pop3-engine.c			\
	camel-pop3-folder.c			\
	camel-pop3-provider.c			\
	camel-pop3-stream.c			\
	camel-pop3-store.c

libcamelpop3include_HEADERS =			\
	camel-pop3-engine.h			\
	camel-pop3-folder.h			\
	camel-pop3-stream.h			\
	camel-pop3-store.h


libcamelpop3_la_LDFLAGS = -avoid-version -module

libcamelpop3_la_LIBADD = $(top_builddir)/e-util/libeutil.la

EXTRA_DIST = libcamelpop3.urls
