## Process this file with automake to produce Makefile.in

SUBDIRS = 

libcamelsmtpincludedir = $(includedir)/camel

providerdir = $(pkglibdir)/camel-providers/$(VERSION)

provider_LTLIBRARIES = libcamelsmtp.la
provider_DATA = libcamelsmtp.urls

INCLUDES =					\
	-I..					\
	-I$(srcdir)/..				\
	-I$(srcdir)/../../..			\
	-I$(includedir)				\
	-I$(top_srcdir)/intl			\
	$(GTK_INCLUDEDIR)			\
	-I$(top_srcdir)/camel

libcamelsmtp_la_SOURCES = 			\
	camel-smtp-provider.c			\
	camel-smtp-transport.c

libcamelsmtpinclude_HEADERS =			\
	camel-smtp-transport.h


libcamelsmtp_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir) 

EXTRA_DIST = libcamelsmtp.urls
