## Process this file with automake to produce Makefile.in

SUBDIRS = 

libcamelmhincludedir = $(includedir)/camel


lib_LTLIBRARIES = libcamelmh.la

INCLUDES = -I.. -I$(srcdir)/.. -I$(includedir)	\
	-I$(top_srcdir)/intl			\
	$(GTK_INCLUDEDIR) -I$(top_srcdir)/camel

libcamelmh_la_SOURCES = 			\
	camel-mh-folder.c			\
	camel-mh-provider.c			\
	camel-mh-store.c			\
	mh-summary.c				\
	mh-uid.c				\
	mh-utils.c

libcamelmhinclude_HEADERS =			\
	camel-mh-folder.h			\
	camel-mh-store.h			\
	mh-summary.c				\
	mh-uid.h				\
	mh-utils.h


libcamelmh_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)


EXTRA_DIST = 
