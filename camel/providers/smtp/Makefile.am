## Process this file with automake to produce Makefile.in

libcamelsmtpincludedir = $(privincludedir)/camel

camel_provider_LTLIBRARIES = libcamelsmtp.la
camel_provider_DATA = libcamelsmtp.urls

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
	$(NSPR_CFLAGS)				\
	$(NSS_CFLAGS)				\
	$(OPENSSL_CFLAGS)			\
	-DG_LOG_DOMAIN=\"camel-smtp-provider\"

libcamelsmtp_la_SOURCES = 			\
	camel-smtp-provider.c			\
	camel-smtp-transport.c

libcamelsmtpinclude_HEADERS =			\
	camel-smtp-transport.h


libcamelsmtp_la_LDFLAGS = -avoid-version -module

libcamelsmtp_la_LIBADD = $(top_builddir)/e-util/libeutil.la

EXTRA_DIST = libcamelsmtp.urls
