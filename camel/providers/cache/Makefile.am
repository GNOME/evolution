## Process this file with automake to produce Makefile.in

SUBDIRS = 

libcamelcacheincludedir = $(includedir)/camel

providerdir = $(pkglibdir)/camel-providers/$(VERSION)

provider_LTLIBRARIES = libcamelcache.la
provider_DATA = libcamelcache.urls

INCLUDES =					\
	-I..					\
	-I$(srcdir)/..				\
	-I$(srcdir)/../../..			\
        -I$(includedir)				\
	-I$(top_srcdir)/intl			\
	$(GTK_INCLUDEDIR)			\
	-I$(top_srcdir)/camel			\
	-DG_LOG_DOMAIN=\"camel-cache-provider\"

libcamelcache_la_SOURCES = 			\
	camel-cache-folder.c			\
	camel-cache-provider.c			\
	camel-cache-store.c			\
	camel-cache-map.c

libcamelcacheinclude_HEADERS =			\
	camel-cache-folder.h			\
	camel-cache-store.h


libcamelcache_la_LDFLAGS = -version-info 0:0:0

EXTRA_DIST = libcamelcache.urls
