## Process this file with automake to produce Makefile.in

SUBDIRS = 

libcamelmboxincludedir = $(includedir)/camel


lib_LTLIBRARIES = libcamelmbox.la

INCLUDES = -I.. -I$(srcdir)/.. -I$(includedir)	\
	-I$(top_srcdir)/intl			\
	$(GTK_INCLUDEDIR) -I$(top_srcdir)/camel

libcamelmbox_la_SOURCES = 			\
	camel-mbox-folder.c			\
	camel-mbox-store.c			\
	camel-mbox-parser.c			\
	camel-mbox-utils.c

libcamelmboxinclude_HEADERS =			\
	camel-mbox-folder.h			\
	camel-mbox-store.h			\
	camel-mbox-parser.h			\
	camel-mbox-utils.h


libcamelmbox_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)


EXTRA_DIST = 
