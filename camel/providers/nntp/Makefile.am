## Process this file with automake to produce Makefile.in

libcamelnntpincludedir = $(includedir)/camel

camel_provider_LTLIBRARIES = libcamelnntp.la
camel_provider_DATA = libcamelnntp.urls

INCLUDES = -I../..				\
	-I$(top_srcdir)/camel			\
	-I$(top_srcdir)/intl			\
	-I$(top_srcdir)/libibex			\
	-I$(top_srcdir)/e-util			\
	-I$(top_srcdir)				\
	-I$(includedir)				\
	$(EXTRA_GNOME_CFLAGS)			\
	-DG_LOG_DOMAIN=\"camel-nntp-provider\"

libcamelnntp_la_SOURCES = 			\
	camel-nntp-provider.c			\
	camel-nntp-store.c			\
	camel-nntp-folder.c			\
	camel-nntp-stream.c			\
	camel-nntp-summary.c

#	camel-nntp-auth.c			\
#	camel-nntp-folder.c			\
#	camel-nntp-grouplist.c			\
#	camel-nntp-newsrc.c			\
#	camel-nntp-provider.c			\
#	camel-nntp-store.c			\
#	camel-nntp-utils.c

libcamelnntpinclude_HEADERS =			\
	camel-nntp-store.h			\
	camel-nntp-folder.h			\
	camel-nntp-stream.h			\
	camel-nntp-summary.h

#	camel-nntp-auth.h			\
#	camel-nntp-folder.h			\
#	camel-nntp-grouplist.h			\
#	camel-nntp-newsrc.h			\
#	camel-nntp-resp-codes.h			\
#	camel-nntp-store.h			\
#	camel-nntp-types.h			\
#	camel-nntp-utils.h

noinst_HEADERS =				\
	camel-nntp-private.h

libcamelnntp_la_LDFLAGS = -version-info 0:0:0

EXTRA_DIST = libcamelnntp.urls
