## Process this file with automake to produce Makefile.in

libcamelmaildirincludedir = $(includedir)/camel

lib_LTLIBRARIES = libcamelmaildir.la

INCLUDES = -I.. -I$(srcdir)/.. -I$(top_srcdir)/intl -I$(top_srcdir)/camel \
	$(GTK_INCLUDEDIR) -I$(includedir)		\
	-DG_LOG_DOMAIN=\"camel-maildir-provider\"		

libcamelmaildir_la_SOURCES = \
	camel-maildir-folder.c \
	camel-maildir-provider.c \
	camel-maildir-store.c

libcamelmaildirinclude_HEADERS = \
	camel-maildir-folder.h \
	camel-maildir-store.h

libcamelmaildir_la_LDFLAGS = -version-info 0:0:0

EXTRA_DIST = 
