## Process this file with automake to produce Makefile.in

camel_provider_LTLIBRARIES = libcamelsmtp.la
camel_provider_DATA = libcamelsmtp.urls

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/intl			\
	-I$(top_srcdir)/camel			\
	$(CAMEL_CFLAGS)				\
	-DG_LOG_DOMAIN=\"camel-smtp-provider\"

libcamelsmtp_la_SOURCES = 			\
	camel-smtp-provider.c			\
	camel-smtp-transport.c

noinst_HEADERS =			\
	camel-smtp-transport.h

libcamelsmtp_la_LDFLAGS = -avoid-version -module

libcamelsmtp_la_LIBADD = \
	$(top_builddir)/libedataserver/libedataserver-${BASE_VERSION}.la

EXTRA_DIST = libcamelsmtp.urls
